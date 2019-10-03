#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <filesystem>
#include <regex>
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <cstdint>
#include <csignal>
#include <unistd.h>
#include <getopt.h>

#include <grpc++/grpc++.h>
#include <spdlog/spdlog.h>
#include <zmq.hpp>
#include <gs++/gs++.hpp>
#include <gs++/bhash.hpp>
#include "graphsearch.grpc.pb.h"

#define ENABLE_SLP
#define ENABLE_UTXO


#ifdef ENABLE_SLP
#include <gs++/mdatabase.hpp>
#include <gs++/txgraph.hpp>
#endif
#ifdef ENABLE_UTXO
#include <gs++/rpc.hpp>
#include <gs++/utxodb.hpp>
#endif

std::unique_ptr<grpc::Server> gserver;
std::atomic<int>  current_block_height    = { -1 };
std::atomic<bool> continue_watching_mongo = { true };
bool exit_early = false;

#ifdef ENABLE_SLP
gs::txgraph g;
#endif

#ifdef ENABLE_UTXO
gs::utxodb utxodb;
#endif


std::filesystem::path get_tokendir(const gs::tokenid tokenid)
{
    const std::string tokenid_str = tokenid.decompress();
    const std::string p1 = tokenid_str.substr(0, 1);
    const std::string p2 = tokenid_str.substr(1, 1);
    return std::filesystem::path("cache") / p1 / p2;
}

void signal_handler(int signal)
{
    spdlog::info("received signal {} requesting to shut down", signal);

    exit_early              = true;
    continue_watching_mongo = false;

    if (gserver) {
        const auto deadline = std::chrono::system_clock::now() +
                              std::chrono::milliseconds(1000);
        gserver->Shutdown(deadline);
    }
}

class GraphSearchServiceImpl final
 : public graphsearch::GraphSearchService::Service
{
#ifdef ENABLE_SLP
    grpc::Status GraphSearch (
        grpc::ServerContext* context,
        const graphsearch::GraphSearchRequest* request,
        graphsearch::GraphSearchReply* reply
    ) override {
        const std::string lookup_txid = request->txid();

        const auto start = std::chrono::steady_clock::now();

        std::pair<gs::graph_search_status, std::vector<std::string>> result;
        // cowardly validating user provided data
        static const std::regex txid_regex("^[0-9a-fA-F]{64}$");
        const bool rmatch = std::regex_match(lookup_txid, txid_regex);
        if (rmatch) {
            result = g.graph_search__ptr(gs::txid(lookup_txid));

            if (result.first == gs::graph_search_status::OK) {
                for (auto i : result.second) {
                    reply->add_txdata(i);
                }
            }
        }

        const auto end = std::chrono::steady_clock::now();
        const auto diff = end - start;
        const auto diff_ms = std::chrono::duration<double, std::milli>(diff).count();

        if (rmatch) {
            spdlog::info("lookup: {} {} ({} ms)", lookup_txid, result.second.size(), diff_ms);

            switch (result.first) {
                case gs::graph_search_status::OK:
                    return { grpc::Status::OK };
                case gs::graph_search_status::NOT_FOUND:
                    return { grpc::StatusCode::NOT_FOUND,
                            "txid not found" };
                case gs::graph_search_status::NOT_IN_TOKENGRAPH:
                    spdlog::error("graph_search__ptr: txid not found in tokengraph {}", lookup_txid);
                    return { grpc::StatusCode::INTERNAL, 
                            "txid found but not in tokengraph" };
                default:
                    spdlog::error("unknown graph_search_status");
                    std::exit(EXIT_FAILURE);
            }
        } else {
            spdlog::info("lookup: {} {} ({} ms)", std::string('*', 64), result.second.size(), diff_ms);
            return { grpc::StatusCode::INVALID_ARGUMENT, "txid did not match regex" };
        }
    }
#endif

#ifdef ENABLE_UTXO
    grpc::Status UtxoSearchByOutpoints (
        grpc::ServerContext* context,
        const graphsearch::UtxoSearchByOutpointsRequest* request,
        graphsearch::UtxoSearchReply* reply
    ) override {
        std::vector<gs::outpoint> outpoints;
        for (auto o : request->outpoints()) {
            outpoints.push_back(gs::outpoint(o.txid(), o.vout()));
        }

        const auto start = std::chrono::steady_clock::now();

        std::vector<gs::output> outputs = utxodb.get_outputs_by_outpoints(outpoints);

        for (auto o : outputs) {
            graphsearch::Output* el = reply->add_outputs();
            el->set_prev_tx_id(o.prev_tx_id.begin(), o.prev_tx_id.size());
            el->set_prev_out_idx(o.prev_out_idx);
            el->set_height(o.height);
            el->set_value(o.value);
            el->set_pk_script(o.pk_script.data(), o.pk_script.size());
        }

        const auto end = std::chrono::steady_clock::now();
        const auto diff = end - start;
        const auto diff_ms = std::chrono::duration<double, std::milli>(diff).count();

        spdlog::info("utxo-outpoints: ({} ms)", diff_ms);
        return { grpc::Status::OK };
    }
#endif
};

int main(int argc, char * argv[])
{
    // std::signal(SIGINT, signal_handler);
    // std::signal(SIGTERM, signal_handler);

    std::string grpc_host = "0.0.0.0";
    std::string grpc_port = "50051";
    std::string mongo_db_name = "slpdb";

    std::string   rpc_host = "0.0.0.0";
    std::uint16_t rpc_port = 8332;
    std::string   rpc_user = "user";
    std::string   rpc_pass = "password";

    bool disable_slpsync          = false;
    bool disable_utxo_chkpnt_load = false;
    bool disable_utxo_chkpnt_save = false;
    bool disable_utxosync         = false;
    bool disable_zmq              = false;
    bool disable_mongowatch       = false;
    bool disable_grpc             = false;

    while (true) {
        static struct option long_options[] = {
            { "help",     no_argument,       nullptr, 'h' },
            { "version",  no_argument,       nullptr, 'v' },
            { "db",       required_argument, nullptr, 90 },
            { "host",     required_argument, nullptr, 100 },
            { "port",     required_argument, nullptr, 101 },
            { "rpc_host", required_argument, nullptr, 1000 },
            { "rpc_port", required_argument, nullptr, 1001 },
            { "rpc_user", required_argument, nullptr, 1002 },
            { "rpc_pass", required_argument, nullptr, 1003 },
            { "disable_slpsync",             no_argument, nullptr, 2000 },
            { "disable_utxo_chkpnt_load",    no_argument, nullptr, 2001 },
            { "disable_utxo_chkpnt_save",    no_argument, nullptr, 2002 },
            { "disable_utxosync",            no_argument, nullptr, 2003 },
            { "disable_zmq",                 no_argument, nullptr, 2004 },
            { "disable_mongowatch",          no_argument, nullptr, 2005 },
            { "disable_grpc",                no_argument, nullptr, 2006 },
        };

        int option_index = 0;
        int c = getopt_long(argc, argv, "hvd:b:p:", long_options, &option_index);

        if (c == -1) {
            break;
        }

        std::stringstream ss(optarg != nullptr ? optarg : "");
        switch (c) {
            case 0:
                if (long_options[option_index].flag != 0) {
                    break;
                }

                break;
            case 'h':
                std::cout <<
                    "usage: gs++ [--version] [--help] [--db db_name]\n"
                    "            [--host host_address] [--port port]\n";
                return EXIT_SUCCESS;
            case 'v':
                std::cout <<
                    "gs++ v" << GS_VERSION << std::endl;
                return EXIT_SUCCESS;

            case 90: ss >> mongo_db_name; break;

            case 100: ss >> grpc_host; break;
            case 101: ss >> grpc_port; break;

            case 1000: ss >> rpc_host; break;
            case 1001: ss >> rpc_port; break;
            case 1002: ss >> rpc_user; break;
            case 1003: ss >> rpc_pass; break;

            case 2000: disable_slpsync          = true; break;
            case 2001: disable_utxo_chkpnt_load = true; break;
            case 2002: disable_utxo_chkpnt_save = true; break;
            case 2003: disable_utxosync         = true; break;
            case 2004: disable_zmq              = true; break;
            case 2005: disable_mongowatch       = true; break;
            case 2006: disable_grpc             = true; break;

            case '?':
                return EXIT_FAILURE;
            default:
                return EXIT_FAILURE;
        }
    }

    spdlog::info("hello");


#ifdef ENABLE_SLP
    gs::mdatabase mdb(mongo_db_name);


    if (! disable_slpsync) {
        spdlog::info("waiting for slpdb to sync...");
        bool running = false;
        while (! running) {
            if (exit_early) {
                return EXIT_SUCCESS;
            }

            current_block_height = mdb.get_current_block_height(running);

            if (current_block_height < 0) {
                std::cerr << "Current block height could not be retrieved.\n"
                             "This can be caused by a few things:\n"
                             "\t* Are you running recent SLPDB version?\n"
                             "\t* Do you have correct database selected?\n";
                return EXIT_FAILURE;
            }

            // skip 1 second delay below
            if (running) {
                break;
            }

            const std::chrono::milliseconds await_time { 1000 };
            std::this_thread::sleep_for(await_time);
        }

        try {
            const std::vector<gs::tokenid> token_ids = mdb.get_all_token_ids();

            unsigned cnt = 0;
            for (const gs::tokenid & tokenid : token_ids) {
                if (exit_early) {
                    return EXIT_SUCCESS;
                }

                auto txs = mdb.load_token(tokenid, current_block_height);
                const unsigned txs_inserted = g.insert_token_data(tokenid, txs);

                ++cnt;
                spdlog::info("loaded: {} {}\t({}/{})", tokenid.decompress(), txs_inserted, cnt, token_ids.size());
            }
        } catch (const std::logic_error& e) {
            spdlog::error(e.what());
            return EXIT_FAILURE;
        }
    }


    std::thread mongo_status_update_thread([&] {
        if (disable_mongowatch) {
            return;
        }

        mdb.watch_for_status_update(
            g,
            current_block_height,
            continue_watching_mongo
        );
    });
#endif

#ifdef ENABLE_UTXO
    // setup utxodb stuff
    gs::rpc rpc(rpc_host, rpc_port, rpc_user, rpc_pass);

    if (! disable_utxo_chkpnt_load) {
        utxodb.load_from_bchd_checkpoint(
            "../utxo-checkpoints/QmXkBQJrMKkCKNbwv4m5xtnqwU9Sq7kucPigvZW8mWxcrv",
            582680, "0000000000000000000000000000000000000000000000000000000000000000"
        );
    }

    if (! disable_utxosync) {
        const std::uint32_t best_block_height = rpc.get_best_block_height();
        std::cout << "best block height: " << best_block_height << "\n";
        for (std::uint32_t h=582680; h<best_block_height; ++h) {
            std::cout << "block: " << h << "\n";
            const std::vector<std::uint8_t> block_data = rpc.get_raw_block(h);
            utxodb.process_block(block_data, true);
        }
    }

    if (! disable_utxo_chkpnt_save) {
        utxodb.save_bchd_checkpoint("../utxo-checkpoints/test");
    }

    std::thread zmq_listener([&] {
        if (disable_zmq) {
            return;
        }
        zmq::context_t context(1);
        zmq::socket_t sock(context, ZMQ_SUB);
        sock.connect("tcp://127.0.0.1:28332");
        sock.setsockopt(ZMQ_SUBSCRIBE, "rawtx", strlen("rawtx"));

        while (true) {
            zmq::message_t env;
            sock.recv(&env);
            std::string env_str = std::string(static_cast<char*>(env.data()), env.size());

            if (env_str == "rawtx" || env_str == "rawblock") {
                std::cout << "Received envelope '" << env_str << "'" << std::endl;

                zmq::message_t msg;
                sock.recv(&msg);

                std::vector<std::uint8_t> msg_data;
                msg_data.reserve(msg.size());

                std::copy(
                    static_cast<std::uint8_t*>(msg.data()),
                    static_cast<std::uint8_t*>(msg.data())+msg.size(),
                    std::back_inserter(msg_data)
                );

                if (env_str == "rawtx") {
                    utxodb.process_mempool_tx(msg_data);
                }
                if (env_str == "rawblock") {
                    utxodb.process_block(msg_data, true);
                }
            }
        }
    });
#endif

    if (! disable_grpc) {
        const std::string server_address(grpc_host+":"+grpc_port);

        GraphSearchServiceImpl graphsearch_service;
        grpc::ServerBuilder builder;
        builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
        builder.RegisterService(&graphsearch_service);
        gserver = builder.BuildAndStart();
        spdlog::info("gs++ listening on {}", server_address);

        if (gserver) {
            gserver->Wait();
        }
    }

#ifdef ENABLE_SLP
    mongo_status_update_thread.join();
#endif

    spdlog::info("goodbye");

    return EXIT_SUCCESS;
}
