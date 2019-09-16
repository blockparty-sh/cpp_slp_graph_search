#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <thread>
#include <filesystem>
#include <cstdlib>
#include <cstdint>
#include <csignal>
#include <cassert>
#include <unistd.h>
#include <getopt.h>

#include <absl/container/node_hash_map.h>
#include <grpc++/grpc++.h>
#include "helloworld.grpc.pb.h"
#include "gs++.hpp"
#include "transaction.hpp"
#include "graph_node.hpp"
#include "txhash.hpp"
#include "mdatabase.hpp"
#include "txgraph.hpp"


std::string grpc_bind = "0.0.0.0";
std::string grpc_port = "50051";
std::unique_ptr<grpc::Server> gserver;
int current_block_height = -1;
txgraph g;


std::filesystem::path get_tokendir(const txhash tokenid)
{
    const std::string p1 = tokenid.substr(0, 1);
    const std::string p2 = tokenid.substr(1, 1);
    return std::filesystem::path("cache") / p1 / p2;
}

void signal_handler(int signal)
{
    if (signal == SIGTERM || signal == SIGINT) {
        std::cout << "received signal " << signal << " requesting to shut down" << std::endl;
        gserver->Shutdown();
    }
}

class GraphSearchServiceImpl final
 : public graphsearch::GraphSearchService::Service
{
    grpc::Status GraphSearch (
        grpc::ServerContext* context,
        const graphsearch::GraphSearchRequest* request,
        graphsearch::GraphSearchReply* reply
    ) override {
        const txhash lookup_txid = request->txid();

        std::stringstream ss;
        ss << "lookup: " << lookup_txid;

        const auto start = std::chrono::steady_clock::now();
        std::vector<std::string> result = g.graph_search__ptr(lookup_txid);
        for (auto i : result) {
            reply->add_txdata(i);
        }
        const auto end = std::chrono::steady_clock::now();
        const auto diff = end - start;

        ss  << "\t" << std::chrono::duration <double, std::milli> (diff).count() << " ms "
            << "(" << result.size() << ")"
            << std::endl;

        std::cout << ss.str();

        return grpc::Status::OK;
    }
};

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
                     "are you running recent slpdb version?\n"
                     "do you have correct database selected?\n";
        return EXIT_FAILURE;
    }


    try {
        const std::vector<std::string> token_ids = mdb.get_all_token_ids();

        unsigned cnt = 0;
        for (auto tokenid : token_ids) {
            std::stringstream ss;
            ss << "loaded: " << tokenid;

            auto txs = mdb.load_token(tokenid, current_block_height);
            const unsigned txs_inserted = g.insert_token_data(tokenid, txs);

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
        mdb.watch_for_status_update(g, current_block_height);
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
