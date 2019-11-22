#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <regex>
#include <atomic>
#include <chrono>
#include <fstream>
#include <iterator>
#include <cstdlib>
#include <cstdint>
#include <csignal>
#include <unistd.h>
#include <getopt.h>

#include <boost/thread.hpp>
#include <boost/filesystem.hpp>
#include <grpc++/grpc++.h>
#include <spdlog/spdlog.h>
#include <zmq.hpp>
#include <libbase64.h>
#include <toml.hpp>

#include "graphsearch.grpc.pb.h"

#include <gs++/bhash.hpp>
#include <gs++/txgraph.hpp>
#include <gs++/rpc.hpp>
#include <gs++/bch.hpp>
#include <gs++/graph_node.hpp>
#include <gs++/transaction.hpp>
#include <gs++/slp_transaction.hpp>
#include <gs++/block.hpp>
#include <gs++/slp_validator.hpp>
#include <gs++/util.hpp>

std::unique_ptr<grpc::Server> gserver;
std::atomic<int>  current_block_height    = { 543375 };
std::atomic<bool> exit_early = { false };

std::atomic<bool> startup_processing_mempool = { true };
std::vector<gs::transaction> startup_mempool_transactions;

bool cache_enabled = false;
boost::filesystem::path cache_dir;


gs::slp_validator validator;
gs::txgraph g;
gs::bch bch;


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

        const gs::txid lookup_txid(request->txid());

        std::pair<gs::graph_search_status, std::vector<std::vector<std::uint8_t>>> result;
        // cowardly validating user provided data
        static const std::regex txid_regex("^[0-9a-fA-F]{64}$");
        const bool rmatch = std::regex_match(request->txid(), txid_regex);
        if (rmatch) {
            result = g.graph_search__ptr(lookup_txid);

            if (result.first == gs::graph_search_status::OK) {
                for (auto & m : result.second) {
                    reply->add_txdata(m.data(), m.size());
                }
            }
        }

        const auto end = std::chrono::steady_clock::now();
        const auto diff = end - start;
        const auto diff_ms = std::chrono::duration<double, std::milli>(diff).count();

        if (rmatch) {
            spdlog::info("lookup: {} {} ({} ms)", lookup_txid.decompress(true), result.second.size(), diff_ms);

            switch (result.first) {
                case gs::graph_search_status::OK:
                    return { grpc::Status::OK };
                case gs::graph_search_status::NOT_FOUND:
                    return { grpc::StatusCode::NOT_FOUND,
                            "txid not found" };
                case gs::graph_search_status::NOT_IN_TOKENGRAPH:
                    spdlog::error("graph_search__ptr: txid not found in tokengraph {}", lookup_txid.decompress(true));
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
            outpoints.emplace_back(o.txid(), o.vout());
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

bool slpsync_bitcoind_process_block(const gs::block& block, const bool mempool)
{
    absl::flat_hash_map<gs::tokenid, std::vector<gs::transaction>> valid_txs;
    for (auto & tx : block.txs) {
        if (validator.transaction_map.count(tx.txid)) {
            // skip over ones we've already added from mempool
            continue;
        }
        if (! validator.add_tx(tx)) {
            // std::cerr << "invalid tx: " << tx.txid.decompress(true) << std::endl;
            continue;
        }

        if (! valid_txs.count(tx.slp.tokenid)) {
            valid_txs.insert({ tx.slp.tokenid, { tx } });
        } else {
            valid_txs[tx.slp.tokenid].push_back(tx);
        }
    }

    for (auto & m : valid_txs) {
        g.insert_token_data(m.first, m.second);
    }

    if (! mempool) {
        spdlog::info("processed block {} ({})", current_block_height, validator.transaction_map.size());
    } else {
        spdlog::info("processed mempool ({})", validator.transaction_map.size());
    }

    return true;
}

bool slpsync_bitcoind_process_tx(const std::vector<std::uint8_t>& txdata)
{
    gs::transaction tx;
    if (! tx.hydrate(txdata.begin(), txdata.end())) {
        std::cerr << "failed to parse tx\n";
        return false; // TODO better handling
    }
    // spdlog::info("zmq-tx {}", tx.txid.decompress(true));

    if (tx.slp.type == gs::slp_transaction_type::invalid) {
        return true;
    }

    if (! validator.add_tx(tx)) {
        // std::cerr << "invalid tx: " << tx.txid.decompress(true) << std::endl;
        return false;
    }

    g.insert_token_data(tx.slp.tokenid, { tx });

    return true;
}

boost::filesystem::path block_height_to_path(const std::uint32_t height)
{
    return cache_dir / "slp" / std::to_string(height / 1000);
}

bool cache_slp_block(const gs::block& block, const std::uint32_t height)
{
    boost::filesystem::path dir = block_height_to_path(height);

    if (! boost::filesystem::exists(dir)) {
        boost::filesystem::create_directories(dir);
    }

    boost::filesystem::path blk_path = dir / std::to_string(height);
    boost::filesystem::ofstream outf(blk_path, boost::filesystem::ofstream::binary);

    outf.write(reinterpret_cast<const char *>(&block.version), sizeof(std::uint32_t));
    outf.write(reinterpret_cast<const char *>(block.prev_block.data()), block.prev_block.size());
    outf.write(reinterpret_cast<const char *>(block.merkle_root.data()), block.merkle_root.size());
    outf.write(reinterpret_cast<const char *>(&block.timestamp), sizeof(std::uint32_t));
    outf.write(reinterpret_cast<const char *>(&block.bits), sizeof(std::uint32_t));
    outf.write(reinterpret_cast<const char *>(&block.nonce), sizeof(std::uint32_t));

    const std::vector<std::uint8_t> varint_tx_length = gs::util::num_to_var_int(block.txs.size());
    outf.write(reinterpret_cast<const char *>(varint_tx_length.data()), varint_tx_length.size());

    for (auto & tx : block.txs) {
        outf.write(reinterpret_cast<const char *>(tx.serialized.data()), tx.serialized.size());
    }

    return true;
}

int main(int argc, char * argv[])
{
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);

    if (argc < 2) {
        std::cerr << "usage: gs++ config.toml\n";
        return EXIT_FAILURE;
    }

    const auto config = toml::parse(argv[1]);
    const bool cache_enabled = toml::find<bool>(config, "services", "cache");
    if (cache_enabled) {
        cache_dir = boost::filesystem::path(toml::find<std::string>(config, "cache", "dir"));
    }


    spdlog::info("hello");

    gs::rpc rpc(
        toml::find<std::string>  (config, "bitcoind", "host"),
        toml::find<std::uint16_t>(config, "bitcoind", "port"),
        toml::find<std::string>  (config, "bitcoind", "user"),
        toml::find<std::string>  (config, "bitcoind", "pass")
    );

    if (toml::find<bool>(config, "services", "utxosync")) {
        if (toml::find<bool>(config, "utxo", "checkpoint_load")) {
        }

        const std::pair<bool, std::uint32_t> best_block_height = rpc.get_best_block_height();
        if (! best_block_height.first) {
            spdlog::error("could not connect to rpc");
            return EXIT_FAILURE;
        }

        spdlog::info("best block height: {}", best_block_height.second);
        for (std::uint32_t h=toml::find<std::uint32_t>(config, "utxo", "block_height"); h<=best_block_height.second; ++h) {
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

        if (toml::find<bool>(config, "utxo", "checkpoint_save")) {
        }
    }

    if (toml::find<bool>(config, "services", "graphsearch")) {
        if (cache_enabled) {
            for (;;++current_block_height) {
                boost::filesystem::path blk_path = block_height_to_path(current_block_height) / std::to_string(current_block_height);
                if (! boost::filesystem::exists(blk_path)) {
                    --current_block_height;
                    break;
                }

                std::ifstream ifs(blk_path.string(), std::ios::in | std::ios::binary);
                const std::vector<std::uint8_t> blk_data((std::istreambuf_iterator<char>(ifs)),
                                                          std::istreambuf_iterator<char>());

                gs::block block;
                block.hydrate(blk_data.begin(), blk_data.end());
                if (! slpsync_bitcoind_process_block(block, false)) {
                    std::cerr << "could not hydrate block\n";
                    --current_block_height;
                    break;
                }
            }
        }
        while (true) {
            const std::pair<bool, std::uint32_t> best_block_height = rpc.get_best_block_height();
            if (! best_block_height.first) {
                spdlog::error("could not connect to rpc");
                return EXIT_FAILURE;
            }

            spdlog::info("best block height: {}", best_block_height.second);

            if (current_block_height == best_block_height.second) {
                break;
            }

            for (
                ;
                current_block_height<=best_block_height.second;
                ++current_block_height
            ) {
                const std::pair<bool, std::vector<std::uint8_t>> block_data = rpc.get_raw_block(current_block_height);
                if (! block_data.first) {
                    spdlog::warn("rpc request failed, trying again...");
                    const std::chrono::milliseconds await_time { 1000 };
                    std::this_thread::sleep_for(await_time);
                    --current_block_height;
                    continue;
                }

                gs::block block;
                if (! block.hydrate(block_data.second.begin(), block_data.second.end(), true)) {
                    std::cerr << "failed to parse block\n";
                    --current_block_height;
                    continue;
                }
                block.topological_sort();
                if (! slpsync_bitcoind_process_block(block, false)) {
                    std::cerr << "failed to process block\n";
                    --current_block_height;
                    continue;
                }

                if (cache_enabled) {
                    cache_slp_block(block, current_block_height);
                }
            }
            current_block_height = best_block_height.second;
        }
    }

    std::thread zmq_listener([&] {
        if (! toml::find<bool>(config, "services", "zmq")) {
            return;
        }
        zmq::context_t context(1);
        zmq::socket_t sock(context, ZMQ_SUB);
        sock.connect(
            "tcp://"+
            toml::find<std::string>(config, "bitcoind", "host")+
            ":"+
            std::to_string(toml::find<std::uint16_t>(config, "bitcoind", "zmq_port"))
        );
        sock.setsockopt(ZMQ_SUBSCRIBE, "", 0);

        while (true) {
            if (exit_early) {
                return;
            }
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


                    if (startup_processing_mempool) {
                        if (env_str == "rawtx") {
                            gs::transaction tx;
                            if (tx.hydrate(msg_data.begin(), msg_data.end())) {
                                if (tx.slp.type != gs::slp_transaction_type::invalid) {
                                    startup_mempool_transactions.push_back(tx);
                                }
                            }
                        }
                    } else {
                        if (env_str == "rawtx") {
                            slpsync_bitcoind_process_tx(msg_data);
                            // bch.process_mempool_tx(msg_data);
                        }
                        if (env_str == "rawblock") {
                            gs::block block;
                            if (! block.hydrate(msg_data.begin(), msg_data.end(), true)) {
                                std::cerr << "failed to parse block\n";
                                continue;
                            }
                            if (! slpsync_bitcoind_process_block(block, false)) {
                                std::cerr << "failed to process block\n";
                                continue;
                            }
                            // bch.process_block(msg_data, true);
                            ++current_block_height;
                        }
                    }
                }
            } catch (const zmq::error_t& e) {
                spdlog::error(e.what());
            }
        }
    });

    if (toml::find<bool>(config, "services", "graphsearch")) {
        // TODO we should start zmq prior to this and have some extensive handling
        // so we dont miss any txs on boot, right now with bad timing this is
        // possible.
        std::pair<bool, std::vector<gs::txid>> txids = rpc.get_raw_mempool();
        if (! txids.first) {
            std::cerr << "canot get mempool\n";
        } else {
            for (const gs::txid & txid : txids.second) {
                const std::pair<bool, std::vector<std::uint8_t>> txdata = rpc.get_raw_transaction(txid);

                if (! txdata.first) {
                    std::cerr << "cannot decode tx\n";
                    continue;
                }
                gs::transaction tx;
                const bool hydration_success = tx.hydrate(txdata.second.begin(), txdata.second.end());

                // std::cout << "hydrated mempool: " << hydration_success << " " << tx.txid.decompress(true) << "\n";

                if (hydration_success) {
                    if (tx.slp.type != gs::slp_transaction_type::invalid) {
                        startup_mempool_transactions.push_back(tx);
                    }
                }
            }
        }
    }


    // repurposing to use as mempool container
    gs::block block;
    block.txs = startup_mempool_transactions;
    block.topological_sort();
    slpsync_bitcoind_process_block(block, true);
    startup_processing_mempool = false;


    if (toml::find<bool>(config, "services", "grpc")) {
        const std::string server_address(
            toml::find<std::string>(config, "grpc", "host")+
            ":"+
            std::to_string(toml::find<std::uint16_t>(config, "grpc", "port"))
        );


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

    zmq_listener.join();

    spdlog::info("goodbye");

    return EXIT_SUCCESS;
}
