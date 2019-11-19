#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <regex>
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <cstdint>
#include <csignal>
#include <unistd.h>
#include <getopt.h>

#include <boost/filesystem.hpp>
#include <boost/thread.hpp>
#include <grpc++/grpc++.h>
#include <spdlog/spdlog.h>
#include <zmq.hpp>
#include <libbase64.h>

#include "gs++.hpp"
#include "graphsearch.grpc.pb.h"

#include <gs++/bhash.hpp>
#include <gs++/mdatabase.hpp>
#include <gs++/txgraph.hpp>
#include <gs++/rpc.hpp>
#include <gs++/bch.hpp>
#include <gs++/gs_tx.hpp>
#include <gs++/graph_node.hpp>
#include <gs++/transaction.hpp>
#include <gs++/slp_transaction.hpp>
#include <gs++/block.hpp>
#include <gs++/slp_validator.hpp>
#include <gs++/util.hpp>

std::unique_ptr<grpc::Server> gserver;
std::atomic<int>  current_block_height    = { -1 };
std::atomic<bool> continue_watching_mongo = { true };
std::atomic<bool> exit_early = { false };

gs::txgraph g;
gs::bch bch;


boost::filesystem::path get_tokendir(const gs::tokenid tokenid)
{
    const std::string tokenid_str = tokenid.decompress();
    const std::string p1 = tokenid_str.substr(0, 1);
    const std::string p2 = tokenid_str.substr(1, 1);
    return boost::filesystem::path("cache") / p1 / p2;
}

bool save_token_to_disk(gs::txgraph & g, const gs::tokenid tokenid)
{
    boost::shared_lock<boost::shared_mutex> lock(g.lookup_mtx);
    std::string tokenid_str = tokenid.decompress();
    spdlog::info("saving token to disk {}", tokenid_str);

    const boost::filesystem::path tokendir = get_tokendir(tokenid_str);
    boost::filesystem::create_directories(tokendir);

    const boost::filesystem::path tokenpath(tokendir / tokenid_str);
    boost::filesystem::ofstream outf(tokenpath, std::ofstream::binary);

    for (auto it : g.tokens[tokenid].graph) {
        auto node = it.second;
        outf.write(reinterpret_cast<const char *>(node.txid.data()), node.txid.size());

        const std::size_t txdata_size = node.txdata.size();
        outf.write(reinterpret_cast<const char *>(&txdata_size), sizeof(std::size_t));

        outf.write(node.txdata.data(), node.txdata.size());

        const std::size_t inputs_size = node.inputs.size();
        outf.write(reinterpret_cast<const char *>(&inputs_size), sizeof(std::size_t));

        for (gs::graph_node* input : node.inputs) {
            outf.write(reinterpret_cast<const char *>(input->txid.data()), input->txid.size());
        }
    }

    return true;
}

std::vector<gs::gs_tx> load_token_from_disk(gs::txgraph & g, const gs::tokenid tokenid)
{
    boost::shared_lock<boost::shared_mutex> lock(g.lookup_mtx);

    boost::filesystem::path tokenpath = get_tokendir(tokenid) / tokenid.decompress();
    boost::filesystem::ifstream file(tokenpath, std::ios::binary);
    spdlog::info("loading token from disk {}", tokenpath.string());
    std::vector<std::uint8_t> fbuf(std::istreambuf_iterator<char>(file), {});
    std::vector<gs::gs_tx> ret;


    auto it = std::begin(fbuf);
    while (it != std::end(fbuf)) {
        gs::txid txid;
        std::copy(it, it+txid.size(), std::begin(txid));
        it += txid.size();

        std::size_t txdata_size;
        std::copy(it, it+sizeof(std::size_t), reinterpret_cast<char*>(&txdata_size));
        it += sizeof(std::size_t);

        std::string txdata(txdata_size, '\0');
        std::copy(it, it+txdata_size, std::begin(txdata));
        it += txdata_size;

        std::size_t inputs_size;
        std::copy(it, it+sizeof(inputs_size), reinterpret_cast<char*>(&inputs_size));
        it += sizeof(inputs_size);

        std::vector<gs::txid> inputs;
        inputs.reserve(inputs_size);
        for (std::size_t i=0; i<inputs_size; ++i) {
            gs::txid input;
            std::copy(it, it+txid.size(), std::begin(input));
            it += txid.size();
            inputs.emplace_back(input);
        }

        ret.emplace_back(gs::gs_tx(txid, txdata, inputs));
    }
    file.close();

    return ret;
}


std::string scriptpubkey_to_base64(const gs::scriptpubkey& pubkey)
{
    std::string b64(pubkey.v.size()*1.5, '\0');
    std::size_t b64_len = 0;
    base64_encode(
        reinterpret_cast<const char*>(pubkey.v.data()),
        pubkey.v.size(),
        const_cast<char *>(b64.data()),
        &b64_len,
        0
    );
    b64.resize(b64_len);
    return b64;
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
    grpc::Status GraphSearch (
        grpc::ServerContext* context,
        const graphsearch::GraphSearchRequest* request,
        graphsearch::GraphSearchReply* reply
    ) override {
        const auto start = std::chrono::steady_clock::now();

        const std::string lookup_txid = request->txid();

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

    grpc::Status UtxoSearchByOutpoints (
        grpc::ServerContext* context,
        const graphsearch::UtxoSearchByOutpointsRequest* request,
        graphsearch::UtxoSearchReply* reply
    ) override {
        const auto start = std::chrono::steady_clock::now();

        std::vector<gs::outpoint> outpoints;
        for (auto o : request->outpoints()) {
            outpoints.push_back(gs::outpoint(o.txid(), o.vout()));
        }

        const std::vector<gs::output> outputs = bch.utxodb.get_outputs_by_outpoints(outpoints);

        for (auto o : outputs) {
            graphsearch::Output* el = reply->add_outputs();
            el->set_prev_tx_id(o.prev_tx_id.begin(), o.prev_tx_id.size());
            el->set_prev_out_idx(o.prev_out_idx);
            el->set_value(o.value);
            el->set_scriptpubkey(o.scriptpubkey.data(), o.scriptpubkey.size());
        }

        const auto end = std::chrono::steady_clock::now();
        const auto diff = end - start;
        const auto diff_ms = std::chrono::duration<double, std::milli>(diff).count();

        spdlog::info("utxo-outpoints: {} ({} ms)", outputs.size(), diff_ms);
        return { grpc::Status::OK };
    }

    grpc::Status UtxoSearchByScriptPubKey (
        grpc::ServerContext* context,
        const graphsearch::UtxoSearchByScriptPubKeyRequest* request,
        graphsearch::UtxoSearchReply* reply
    ) override {
        const auto start = std::chrono::steady_clock::now();

        const gs::scriptpubkey scriptpubkey = gs::scriptpubkey(request->scriptpubkey());
        const std::uint32_t limit = request->limit();
        const std::vector<gs::output> outputs = bch.utxodb.get_outputs_by_scriptpubkey(scriptpubkey, limit);

        for (auto o : outputs) {
            graphsearch::Output* el = reply->add_outputs();
            el->set_prev_tx_id(o.prev_tx_id.begin(), o.prev_tx_id.size());
            el->set_prev_out_idx(o.prev_out_idx);
            el->set_value(o.value);
            el->set_scriptpubkey(o.scriptpubkey.data(), o.scriptpubkey.size());
        }

        const auto end = std::chrono::steady_clock::now();
        const auto diff = end - start;
        const auto diff_ms = std::chrono::duration<double, std::milli>(diff).count();

        spdlog::info("utxo-scriptpubkey: {} {} ({} ms)", scriptpubkey_to_base64(scriptpubkey), outputs.size(), diff_ms);
        return { grpc::Status::OK };
    }
    
    grpc::Status BalanceByScriptPubKey (
        grpc::ServerContext* context,
        const graphsearch::BalanceByScriptPubKeyRequest* request,
        graphsearch::BalanceByScriptPubKeyReply* reply
    ) override {
        const auto start = std::chrono::steady_clock::now();

        const gs::scriptpubkey scriptpubkey = gs::scriptpubkey(request->scriptpubkey());
        const std::uint64_t balance = bch.utxodb.get_balance_by_scriptpubkey(scriptpubkey);

        reply->set_balance(balance);

        const auto end = std::chrono::steady_clock::now();
        const auto diff = end - start;
        const auto diff_ms = std::chrono::duration<double, std::milli>(diff).count();

        spdlog::info("balance-scriptpubkey: {} {} ({} ms)", scriptpubkey_to_base64(scriptpubkey), balance, diff_ms);
        return { grpc::Status::OK };
    }
};

void slpsync_bitcoind_process_block(const std::vector<std::uint8_t>& block_data)
{
    static gs::slp_validator validator;

    gs::block block;
    if (! block.hydrate(block_data.begin(), block_data.end())) {
        std::cerr << "failed to parse block\n";
    }

    std::vector<gs::transaction> slp_txs;
    for (auto & tx : block.txs) {
        if (tx.slp.type != gs::slp_transaction_type::invalid) {
            slp_txs.push_back(tx);
        }
    }
    slp_txs = gs::util::topological_sort(slp_txs);

    absl::flat_hash_map<gs::tokenid, std::vector<gs::transaction>> valid_txs;
    for (auto & tx : slp_txs) {
        if (! validator.add_tx(tx)) {
            std::cerr << "invalid tx: " << tx.txid.decompress(true) << std::endl;
            continue;
        }

        if (! valid_txs.count(tx.slp.tokenid)) {
            valid_txs.insert({ tx.slp.tokenid, { tx } });
        } else {
            valid_txs[tx.slp.tokenid].push_back(tx);
        }
    }

    for (auto & m : valid_txs) {
        std::vector<gs::gs_tx> gs_txs;
        std::transform(
            m.second.begin(),
            m.second.end(),
            std::back_inserter(gs_txs),
            [](const gs::transaction& tx) -> gs::gs_tx {
                std::vector<gs::txid> inputs;
                std::transform(
                    tx.inputs.begin(),
                    tx.inputs.end(),
                    std::back_inserter(inputs),
                    [](const gs::outpoint & outpoint) -> gs::txid {
                        return outpoint.txid;
                    }
                );

                std::string txdata(tx.serialized.begin(), tx.serialized.end());

                return gs::gs_tx(
                    tx.txid,
                    txdata,
                    inputs
                );
            }
        );

        g.insert_token_data(m.first, gs_txs);
    }
}


int main(int argc, char * argv[])
{
    // std::signal(SIGINT, signal_handler);
    // std::signal(SIGTERM, signal_handler);

    std::string grpc_host = "0.0.0.0";
    std::string grpc_port = "50051";
    std::string mongo_db_name = "slpdb";
    std::string mongo_uri     = "";

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
    bool disable_slpbitcoindsync  = false;

    std::string   utxo_chkpnt_file         = "../utxo-checkpoints/latest";
    std::uint32_t utxo_chkpnt_block_height = 0;
    std::string   utxo_chkpnt_block_hash   = "";

    while (true) {
        static struct option long_options[] = {
            { "help",      no_argument,       nullptr, 'h' },
            { "version",   no_argument,       nullptr, 'v' },
            { "mongo_db",  required_argument, nullptr, 90 },
            { "mongo_uri", required_argument, nullptr, 91 },
            { "host",      required_argument, nullptr, 100 },
            { "port",      required_argument, nullptr, 101 },
            { "rpc_host",  required_argument, nullptr, 1000 },
            { "rpc_port",  required_argument, nullptr, 1001 },
            { "rpc_user",  required_argument, nullptr, 1002 },
            { "rpc_pass",  required_argument, nullptr, 1003 },
            { "disable_slpsync",             no_argument, nullptr, 2000 },
            { "disable_utxo_chkpnt_load",    no_argument, nullptr, 2001 },
            { "disable_utxo_chkpnt_save",    no_argument, nullptr, 2002 },
            { "disable_utxosync",            no_argument, nullptr, 2003 },
            { "disable_zmq",                 no_argument, nullptr, 2004 },
            { "disable_mongowatch",          no_argument, nullptr, 2005 },
            { "disable_grpc",                no_argument, nullptr, 2006 },
            { "disable_slpbitcoindsync",     no_argument, nullptr, 2007 },
            { "utxo_chkpnt_file",         required_argument, nullptr, 3000 },
            { "utxo_chkpnt_block_height", required_argument, nullptr, 3001 },
            { "utxo_chkpnt_block_hash",   required_argument, nullptr, 3002 },
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
            case 91: ss >> mongo_uri; break;

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
            case 2007: disable_slpbitcoindsync  = true; break;

            case 3000: ss >> utxo_chkpnt_file;         break;
            case 3001: ss >> utxo_chkpnt_block_height; break;
            case 3002: ss >> utxo_chkpnt_block_hash;   break;

            case '?':
                return EXIT_FAILURE;
            default:
                return EXIT_FAILURE;
        }
    }

    spdlog::info("hello");

    if (disable_slpsync)          std::cout << "disable_slpsync\n";
    if (disable_utxo_chkpnt_load) std::cout << "disable_utxo_chkpnt_load\n";
    if (disable_utxo_chkpnt_save) std::cout << "disable_utxo_chkpnt_save\n";
    if (disable_utxosync)         std::cout << "disable_utxosync\n";
    if (disable_zmq)              std::cout << "disable_zmq\n";
    if (disable_mongowatch)       std::cout << "disable_mongowatch\n";
    if (disable_grpc)             std::cout << "disable_grpc\n";
    if (disable_slpbitcoindsync)  std::cout << "disable_slpbitcoindsync\n";


    gs::mdatabase mdb(mongo_db_name, mongo_uri);


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

    // setup utxodb stuff
    gs::rpc rpc(rpc_host, rpc_port, rpc_user, rpc_pass);

    if (! disable_slpbitcoindsync) {
        const std::pair<bool, std::uint32_t> best_block_height = rpc.get_best_block_height();
        if (! best_block_height.first) {
            spdlog::error("could not connect to rpc");
            return EXIT_FAILURE;
        }

        spdlog::info("best block height: {}", best_block_height.second);

        constexpr std::uint32_t slp_start_block = 543375;
        for (std::uint32_t h=slp_start_block; h<=best_block_height.second; ++h) {
            const std::pair<bool, std::vector<std::uint8_t>> block_data = rpc.get_raw_block(h);
            if (! block_data.first) {
                spdlog::warn("rpc request failed, trying again...");
                const std::chrono::milliseconds await_time { 1000 };
                std::this_thread::sleep_for(await_time);
                --h;
                continue;
            }
            spdlog::info("processing block {}", h);
            slpsync_bitcoind_process_block(block_data.second);
        }

    }

    if (! disable_utxo_chkpnt_load) {
        bch.utxodb.load_from_bchd_checkpoint(
            utxo_chkpnt_file,
            utxo_chkpnt_block_height,
            utxo_chkpnt_block_hash
        );
    }

    if (! disable_utxosync) {
        const std::pair<bool, std::uint32_t> best_block_height = rpc.get_best_block_height();
        if (! best_block_height.first) {
            spdlog::error("could not connect to rpc");
            return EXIT_FAILURE;
        }

        spdlog::info("best block height: {}", best_block_height.second);
        for (std::uint32_t h=utxo_chkpnt_block_height; h<=best_block_height.second; ++h) {
            const std::pair<bool, std::vector<std::uint8_t>> block_data = rpc.get_raw_block(h);
            if (! block_data.first) {
                spdlog::warn("rpc request failed, trying again...");
                const std::chrono::milliseconds await_time { 1000 };
                std::this_thread::sleep_for(await_time);
                --h;
                continue;
            }
            spdlog::info("processing block {}", h);
            bch.process_block(block_data.second, true);
        }
    }

    if (! disable_utxo_chkpnt_save) {
        bch.utxodb.save_bchd_checkpoint("../utxo-checkpoints/test");
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

    std::thread zmq_listener([&] {
        if (disable_zmq) {
            return;
        }
        zmq::context_t context(1);
        zmq::socket_t sock(context, ZMQ_SUB);
        sock.connect("tcp://127.0.0.1:28332");
        sock.setsockopt(ZMQ_SUBSCRIBE, "rawtx", strlen("rawtx"));

        while (true) {
            try {
                zmq::message_t env;
                sock.recv(&env);
                std::string env_str = std::string(static_cast<char*>(env.data()), env.size());

                if (env_str == "rawtx" || env_str == "rawblock") {
                    // std::cout << "Received envelope '" << env_str << "'" << std::endl;

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
                        bch.process_mempool_tx(msg_data);
                    }
                    if (env_str == "rawblock") {
                        bch.process_block(msg_data, true);
                    }
                }
            } catch (const zmq::error_t& e) {
                spdlog::error(e.what());
            }
        }
    });

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

    mongo_status_update_thread.join();

    spdlog::info("goodbye");

    return EXIT_SUCCESS;
}
