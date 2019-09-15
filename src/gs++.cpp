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
#include <bsoncxx/builder/stream/document.hpp>
#include <bsoncxx/string/to_string.hpp>
#include <bsoncxx/json.hpp>
#include <bsoncxx/types.hpp>
#include <mongocxx/client.hpp>
#include <mongocxx/uri.hpp>
#include <mongocxx/pool.hpp>
#include <mongocxx/instance.hpp>


#include "helloworld.grpc.pb.h"
#include "gs++.hpp"
#include "transaction.hpp"
#include "token_details.hpp"
#include "graph_node.hpp"
#include "txhash.hpp"
#include "graph_search_service.hpp"


std::unique_ptr<grpc::Server> gserver;
std::int32_t current_block_height = -1;
bool continue_watching_mongo = true;

// IMPORTANT: tokens and txid_to_token must be guarded with the lookup_mtx
absl::node_hash_map<txhash, token_details>  tokens;        // tokenid -> token
absl::node_hash_map<txhash, token_details*> txid_to_token; // txid -> token
std::shared_mutex lookup_mtx;

std::string mongo_db_name = "slpdb";
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
    std::string p1 = tokenid.substr(0, 1);
    std::string p2 = tokenid.substr(1, 1);
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
    std::unique_lock lock(lookup_mtx);

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


std::vector<txhash> get_all_token_ids_from_mongo(
    mongocxx::pool & pool
) {
    auto client = pool.acquire();
    auto collection = (*client)[mongo_db_name]["tokens"];

    mongocxx::options::find opts{};
    opts.projection(
        bsoncxx::builder::stream::document{}
    << "tokenDetails.tokenIdHex" << 1
    << bsoncxx::builder::stream::finalize
    );

    std::vector<txhash> ret;
    auto cursor = collection.find({}, opts);
    for (auto&& doc : cursor) {
        const auto el = doc["tokenDetails"]["tokenIdHex"];
        assert(el.type() == bsoncxx::type::k_utf8);
        const std::string tokenIdHexStr = bsoncxx::string::to_string(el.get_utf8().value);
        ret.emplace_back(tokenIdHexStr);
    }

    return ret;
}

std::int32_t get_current_block_height_from_mongo(
    mongocxx::pool & pool
) {
    auto client = pool.acquire();
    auto collection = (*client)[mongo_db_name]["statuses"];

    mongocxx::options::find opts{};
    opts.projection(
        bsoncxx::builder::stream::document{}
    << "blockHeight" << 1
    << bsoncxx::builder::stream::finalize
    );

    auto cursor = collection.find({}, opts);

    for (auto&& doc : cursor) {
        auto el = doc["blockHeight"];
        assert(el.type() == bsoncxx::type::k_int32 || el.type() == bsoncxx::type::k_int64);

        if (el.type() == bsoncxx::type::k_int32) {
            return el.get_int32().value;
        }

        if (el.type() == bsoncxx::type::k_int64) {
            return el.get_int64().value;
        }
    }

    return -1;
}

void watch_mongo_for_status_update(
    mongocxx::pool & pool,
    std::int32_t & current_block_height
) {
    const std::chrono::milliseconds await_time { 1000 };
    auto client = pool.acquire();
    auto collection = (*client)[mongo_db_name]["statuses"];

    while (continue_watching_mongo) {
        const std::int32_t block_height = get_current_block_height_from_mongo(pool);

        if (block_height > 0 && current_block_height < block_height) {
            for (std::size_t h=current_block_height+1; h<=block_height; ++h) {
                absl::flat_hash_map<txhash, std::vector<transaction>> block_data = load_block_from_mongo(pool, h);
                std::size_t tid = 1;

                for (auto it : block_data) {
                    std::stringstream ss;
                    ss 
                        << "block: " << h
                        << " token: " << it.first
                        << "\t" << insert_token_data(it.first, it.second)
                        << "\t(" << tid << "/" << block_data.size() << ")"
                        << std::endl;
                    std::cout << ss.str();
                    ++tid;
                }

                current_block_height = h;
            }
        }

        std::this_thread::sleep_for(await_time);
    }
}

std::vector<transaction> load_token_from_mongo(
    mongocxx::pool & pool,
    const txhash tokenid,
    const int max_block_height
) {
    using bsoncxx::builder::basic::make_document;
    using bsoncxx::builder::basic::kvp;

    auto client = pool.acquire();
    auto collection = (*client)[mongo_db_name]["graphs"];

    mongocxx::pipeline pipe{};
    pipe.match(make_document(
        kvp("tokenDetails.tokenIdHex", tokenid)
    ));
    pipe.lookup(make_document(
        kvp("from", "confirmed"),
        kvp("localField", "graphTxn.txid"),
        kvp("foreignField", "tx.h"),
        kvp("as", "tx")
    ));

    if (max_block_height >= 0) {
        pipe.match(make_document(
            kvp("tx.blk.i", make_document(
                kvp("$lte", max_block_height)
            ))
        ));
    }

    pipe.project(make_document(
        kvp("graphTxn.txid", 1),
        kvp("graphTxn.inputs.txid", 1),
        kvp("tx.tx.raw", 1)
    ));

    std::vector<transaction> ret;
    auto cursor = collection.aggregate(pipe, mongocxx::options::aggregate{});
    for (auto&& doc : cursor) {
        const auto txid_el = doc["graphTxn"]["txid"];
        assert(txid_el.type() == bsoncxx::type::k_utf8);
        const std::string txidStr = bsoncxx::string::to_string(txid_el.get_utf8().value);

        std::string txdataStr;

        const auto tx_el = doc["tx"];
        const bsoncxx::array::view tx_sarr { tx_el.get_array().value };

        if (tx_sarr.empty()) {
            std::stringstream ss;
            ss << "load_token_from_mongo: associated tx not found in confirmed " << txidStr
               << "\n";
            std::cerr << ss.str();
            continue;
        }
        for (bsoncxx::array::element tx_s_el : tx_sarr) {
            auto txdata_el = tx_s_el["tx"]["raw"];
            assert(txdata_el.type() == bsoncxx::type::k_binary);
            auto txdata_bin = txdata_el.get_binary();

            txdataStr.resize(txdata_bin.size, '\0');
            std::copy(
                txdata_bin.bytes,
                txdata_bin.bytes+txdata_bin.size,
                std::begin(txdataStr)
            );

            break; // this is used for $lookup so just 1 item
        }

        std::vector<txhash> inputs;
        const auto inputs_el = doc["graphTxn"]["inputs"];
        const bsoncxx::array::view inputs_sarr { inputs_el.get_array().value };

        for (bsoncxx::array::element input_s_el : inputs_sarr) {
            auto input_txid_el = input_s_el["txid"];
            assert(input_txid_el.type() == bsoncxx::type::k_utf8);
            const std::string input_txidStr = bsoncxx::string::to_string(input_txid_el.get_utf8().value);
            inputs.emplace_back(input_txidStr);
        }

        ret.emplace_back(transaction(txidStr, txdataStr, inputs));
    }

    return ret;
}

absl::flat_hash_map<txhash, std::vector<transaction>> load_block_from_mongo(
    mongocxx::pool & pool,
    const std::int32_t block_height
) {
    using bsoncxx::builder::basic::make_document;
    using bsoncxx::builder::basic::kvp;

    auto client = pool.acquire();
    auto collection = (*client)[mongo_db_name]["confirmed"];

    mongocxx::pipeline pipe{};
    pipe.match(make_document(
        kvp("blk.i", block_height),
        kvp("slp.valid", true)
    ));
    pipe.lookup(make_document(
        kvp("from", "graphs"),
        kvp("localField", "tx.h"),
        kvp("foreignField", "graphTxn.txid"),
        kvp("as", "graph")
    ));
    pipe.project(make_document(
        kvp("tx.h", 1),
        kvp("tx.raw", 1),
        kvp("slp.detail.tokenIdHex", 1),
        kvp("graph.graphTxn.inputs.txid", 1)
    ));

    absl::flat_hash_map<txhash, std::vector<transaction>> ret;
    auto cursor = collection.aggregate(pipe, mongocxx::options::aggregate{});
    for (auto&& doc : cursor) {
        const auto txid_el = doc["tx"]["h"];
        assert(txid_el.type() == bsoncxx::type::k_utf8);
        const std::string txidStr = bsoncxx::string::to_string(txid_el.get_utf8().value);

        const auto txdata_el = doc["tx"]["raw"];
        assert(txdata_el.type() == bsoncxx::type::k_binary);

        auto txdata_bin = txdata_el.get_binary();

        std::string txdataStr(txdata_bin.size, '\0');
        std::copy(
            txdata_bin.bytes,
            txdata_bin.bytes+txdata_bin.size,
            std::begin(txdataStr)
        );

        const auto tokenidhex_el = doc["slp"]["detail"]["tokenIdHex"];
        assert(tokenidhex_el.type() == bsoncxx::type::k_utf8);
        const std::string tokenidhexStr = bsoncxx::string::to_string(tokenidhex_el.get_utf8().value);


        const auto graph_el = doc["graph"];
        const bsoncxx::array::view graph_sarr { graph_el.get_array().value };

        if (graph_sarr.empty()) {
            std::stringstream ss;
            ss << "load_block_from_mongo: associated tx not found in graphs " << txidStr
               << "\n";
            std::cerr << ss.str();
            continue;
        }
        for (bsoncxx::array::element graph_s_el : graph_sarr) {
            std::vector<txhash> inputs;

            const auto inputs_el = graph_s_el["graphTxn"]["inputs"];
            const bsoncxx::array::view inputs_sarr { inputs_el.get_array().value };

            for (bsoncxx::array::element input_s_el : inputs_sarr) {
                auto input_txid_el = input_s_el["txid"];
                assert(input_txid_el.type() == bsoncxx::type::k_utf8);
                const std::string input_txidStr = bsoncxx::string::to_string(input_txid_el.get_utf8().value);
                inputs.emplace_back(input_txidStr);
            }

            if (! ret.count(tokenidhexStr)) {
                ret.insert({ tokenidhexStr, {} });
            }

            ret[tokenidhexStr].emplace_back(transaction(txidStr, txdataStr, inputs));

            break; // this is used for $lookup so just 1 item
        }
    }

    return ret;
}

void signal_handler(int signal)
{
    if (signal == SIGTERM || signal == SIGINT) {
        std::cout << "received signal " << signal << " requesting to shut down" << std::endl;
        continue_watching_mongo = false;
        gserver->Shutdown();
    }
}

int main(int argc, char * argv[])
{
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


    mongocxx::instance inst{};
    mongocxx::pool pool{mongocxx::uri{}};

    bsoncxx::builder::stream::document document{};

    current_block_height = get_current_block_height_from_mongo(pool);
    if (current_block_height < 0) {
        std::cerr << "current block height could not be retrieved\n"
                     "are you running recent slpdb version?\n";
        return EXIT_FAILURE;
    }


    try {
        const std::vector<std::string> token_ids = get_all_token_ids_from_mongo(pool);

        std::size_t cnt = 0;
        for (auto tokenid : token_ids) {
            std::stringstream ss;
            ss << "loaded: " << tokenid;

            auto txs = load_token_from_mongo(pool, tokenid, current_block_height);
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

    /*
    for (std::size_t bh=590000; bh<600400; ++bh) {
        absl::flat_hash_map<txhash, std::vector<transaction>> block_data =
            load_block_from_mongo(pool, bh);

        std::size_t tid = 1;
        for (auto it : block_data) {
            std::cout
                << "block: " << bh
                << " token: " << it.first
                << "\t" << insert_token_data(it.first, it.second)
                << "\t(" << tid << "/" << block_data.size() << ")"
                << std::endl;
            ++tid;
        }
    }
    */

    /*
    std::cout << "block_data: " << block_data.size() << std::endl;
    for (auto it : block_data) {
        std::cout << "tokenid: " << it.first << std::endl;
        for(auto m : it.second) {
            std::cout << "\t" << m.txid << std::endl;
            std::cout << "\t" << m.txdata.size() << std::endl;
            for(auto x : m.inputs) {
                std::cout << "\t\t" << x << std::endl;
            }
        }

        std::cout << "$$$600188---inserted: " << insert_token_data(it.first, it.second) << std::endl;
    }*/

    std::thread([&] {
        watch_mongo_for_status_update(pool, current_block_height);
    }).detach();


    std::string server_address(grpc_bind+":"+grpc_port);

    GraphSearchServiceImpl graphsearch_service;
    grpc::ServerBuilder builder;
    builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
    builder.RegisterService(&graphsearch_service);
    std::unique_ptr<grpc::Server> gserver(builder.BuildAndStart());
    std::cout << "gs++ listening on " << server_address << std::endl;

    gserver->Wait();

    return EXIT_SUCCESS;
}
