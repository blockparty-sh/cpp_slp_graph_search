#include <iostream>
#include <string>
#include <vector>
#include <fstream>
#include <filesystem>
#include <sstream>
#include <thread>
#include <mutex>
#include <shared_mutex>
#include <cstdlib>
#include <cstdint>
#include <csignal>
#include <cassert>
#include <unistd.h>
#include <getopt.h>

#include <absl/container/node_hash_map.h>
#include <absl/container/flat_hash_set.h>
#include <absl/container/flat_hash_map.h>
#include <grpc++/grpc++.h>
#include "helloworld.grpc.pb.h"
#include "gs++.hpp"
#include "transaction.hpp"
#include "token_details.hpp"
#include "graph_node.hpp"
#include "txhash.hpp"
#include "graph_search_service.hpp"
#include "mdatabase.hpp"


std::unique_ptr<grpc::Server> gserver;
std::int32_t current_block_height = -1;

// IMPORTANT: tokens and txid_to_token must be guarded with the lookup_mtx
absl::node_hash_map<txhash, token_details>  tokens;        // tokenid -> token
absl::node_hash_map<txhash, token_details*> txid_to_token; // txid -> token
std::shared_mutex lookup_mtx;

std::string grpc_bind = "0.0.0.0";
std::string grpc_port = "50051";

void recursive_walk__ptr (
    const graph_node* node,
    absl::flat_hash_set<const graph_node*> & seen
) {
    for (const graph_node* n : node->inputs) {
        if (! seen.count(n)) {
            seen.insert(n);
            recursive_walk__ptr(n, seen);
        }
    }
}

std::vector<std::string> graph_search__ptr(const txhash lookup_txid)
{
    std::shared_lock lock(lookup_mtx);

    if (txid_to_token.count(lookup_txid) == 0) {
        // txid hasn't entered our system yet
        return {};
    }

    token_details* token = txid_to_token[lookup_txid];

    absl::flat_hash_set<const graph_node*> seen;

    if (token->graph.count(lookup_txid) == 0) {
        std::stringstream ss;
        ss << "graph_search__ptr: txid not found in tokengraph " << lookup_txid
           << "\n";
        std::cerr << ss.str();
        return {};
    }
    recursive_walk__ptr(&token->graph[lookup_txid], seen);

    std::vector<std::string> ret;
    ret.reserve(seen.size());

    for (auto it = std::begin(seen); it != std::end(seen); ) {
        ret.emplace_back(std::move(seen.extract(it++).value())->txdata);
    }

    return ret;
}

std::filesystem::path get_tokendir(const txhash tokenid)
{
    const std::string p1 = tokenid.substr(0, 1);
    const std::string p2 = tokenid.substr(1, 1);
    return std::filesystem::path("cache") / p1 / p2;
}


// TODO save writes into buffer to prevent many tiny writes
// should improve performance
bool save_token_to_disk(const txhash tokenid)
{
    std::cout << "saving token to disk" << tokenid;

    const std::filesystem::path tokendir = get_tokendir(tokenid);
    std::filesystem::create_directories(tokendir);

    const std::filesystem::path tokenpath(tokendir / tokenid);
    std::ofstream outf(tokenpath, std::ofstream::binary);

    for (auto it : tokens[tokenid].graph) {
        auto node = it.second;
        outf.write(node.txid.data(), node.txid.size());

        const std::size_t txdata_size = node.txdata.size();
        outf.write(reinterpret_cast<const char *>(&txdata_size), sizeof(std::size_t));

        outf.write(node.txdata.data(), node.txdata.size());

        const std::size_t inputs_size = node.inputs.size();
        outf.write(reinterpret_cast<const char *>(&inputs_size), sizeof(std::size_t));

        for (graph_node* input : node.inputs) {
            outf.write(input->txid.data(), input->txid.size());
        }
    }

    std::cout << "\t done" << tokenpath << std::endl;

    return true;
}

void clear_token_data (const txhash tokenid)
{
    if (tokens.count(tokenid)) {
        tokens.erase(tokenid);
    }
}

std::size_t insert_token_data (
    const txhash tokenid,
    std::vector<transaction> txs
) {
    std::lock_guard lock(lookup_mtx);

    token_details& token = tokens[tokenid];

    if (! tokens.count(tokenid)) {
        tokens.insert({ tokenid, token_details(tokenid) });
    }

    absl::flat_hash_map<txhash, std::vector<txhash>> input_map;

    std::size_t ret = 0;

    // first pass to populate graph nodes
    std::vector<graph_node*> latest;
    latest.reserve(txs.size());
    for (auto tx : txs) {
        if (! txid_to_token.count(tx.txid)) {
            token.graph.insert({ tx.txid, graph_node(tx.txid, tx.txdata) });
            txid_to_token.insert({ tx.txid, &token });
            input_map.insert({ tx.txid, tx.inputs });
            latest.emplace_back(&token.graph[tx.txid]);
            ++ret;
        }
    }

    // second pass to add inputs
    for (graph_node * node : latest) {
        for (const txhash input_txid : input_map[node->txid]) {
            if (! token.graph.count(input_txid)) {
                /*
                std::stringstream ss;
                ss << "insert_token_data: input_txid not found in tokengraph " << input_txid
                   << "\n";
                std::cerr << ss.str();
                */
                continue;
            }

            node->inputs.emplace_back(&token.graph[input_txid]);
        }
    }

    return ret;
}

std::vector<transaction> load_token_from_disk(const txhash tokenid)
{
    constexpr std::size_t txid_size = 64;

    std::filesystem::path tokenpath = get_tokendir(tokenid) / tokenid;
    std::ifstream file(tokenpath, std::ios::binary);
    std::cout << "loading token from disk: " << tokenpath << std::endl;
    std::vector<std::uint8_t> fbuf(std::istreambuf_iterator<char>(file), {});
    std::vector<transaction> ret;


    auto it = std::begin(fbuf);
    while (it != std::end(fbuf)) {
        txhash txid(txid_size, '\0');
        std::copy(it, it+txid_size, std::begin(txid));
        it += txid_size;

        std::size_t txdata_size;
        std::copy(it, it+sizeof(std::size_t), reinterpret_cast<char*>(&txdata_size));
        it += sizeof(std::size_t);

        std::string txdata(txdata_size, '\0');
        std::copy(it, it+txdata_size, std::begin(txdata));
        it += txdata_size;

        std::size_t inputs_size;
        std::copy(it, it+sizeof(inputs_size), reinterpret_cast<char*>(&inputs_size));
        it += sizeof(inputs_size);

        std::vector<txhash> inputs;
        inputs.reserve(inputs_size);
        for (std::size_t i=0; i<inputs_size; ++i) {
            txhash input(txid_size, '\0');
            std::copy(it, it+txid_size, std::begin(input));
            it += txid_size;
            inputs.emplace_back(input);
        }

        ret.emplace_back(transaction(txid, txdata, inputs));
    }
    file.close();

    return ret;
}


void signal_handler(int signal)
{
    if (signal == SIGTERM || signal == SIGINT) {
        std::cout << "received signal " << signal << " requesting to shut down" << std::endl;
        gserver->Shutdown();
    }
}

int main(int argc, char * argv[])
{
    std::string mongo_db_name = "slpdb";
    while (true) {
        static struct option long_options[] = {
            { "help",    no_argument,       nullptr, 'h' },
            { "version", no_argument,       nullptr, 'v' },
            { "db",      required_argument, nullptr, 'd' },
            { "bind",    required_argument, nullptr, 'b' },
            { "port",    required_argument, nullptr, 'p' },
        };

        int option_index = 0;
        int c = getopt_long(argc, argv, "hvd:b:p:", long_options, &option_index);

        if (c == -1) {
            break;
        }

        switch (c) {
            case 0:
                if (long_options[option_index].flag != 0) {
                    break;
                }

                break;
            case 'h':
                std::cout <<
                    "usage: gs++ [--version] [--help] [--db db_name]\n"
                    "            [--bind bind_address] [--port port]\n";
                return EXIT_SUCCESS;
            case 'v':
                std::cout <<
                    "gs++ v" << GS_VERSION << std::endl;
                return EXIT_SUCCESS;
            case 'd':
                mongo_db_name = optarg;
                break;
            case 'b':
                grpc_bind = optarg;
                break;
            case 'p':
                grpc_port = optarg;
                break;
            case '?':
                return EXIT_FAILURE;
            default:
                return EXIT_FAILURE;
        }
    }

    mdatabase mdb(mongo_db_name);


    current_block_height = mdb.get_current_block_height();
    if (current_block_height < 0) {
        std::cerr << "current block height could not be retrieved\n"
                     "are you running recent slpdb version?\n";
        return EXIT_FAILURE;
    }


    try {
        const std::vector<std::string> token_ids = mdb.get_all_token_ids();

        std::size_t cnt = 0;
        for (auto tokenid : token_ids) {
            std::stringstream ss;
            ss << "loaded: " << tokenid;

            auto txs = mdb.load_token(tokenid, current_block_height);
            const std::size_t txs_inserted = insert_token_data(tokenid, txs);

            ++cnt;
            ss 
                << "\t" << txs_inserted
                << "\t(" << cnt << "/" << token_ids.size() << ")"
                << "\n";
            std::cout << ss.str();
        }
    } catch (const std::logic_error& e) {
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    }


    std::thread([&] {
        mdb.watch_for_status_update(current_block_height);
    }).detach();


    const std::string server_address(grpc_bind+":"+grpc_port);

    GraphSearchServiceImpl graphsearch_service;
    grpc::ServerBuilder builder;
    builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
    builder.RegisterService(&graphsearch_service);
    std::unique_ptr<grpc::Server> gserver(builder.BuildAndStart());
    std::cout << "gs++ listening on " << server_address << std::endl;

    gserver->Wait();

    return EXIT_SUCCESS;
}
