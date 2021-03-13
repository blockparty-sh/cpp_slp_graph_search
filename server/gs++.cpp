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

#include <boost/thread.hpp>
#include <boost/filesystem.hpp>
#include <grpc++/grpc++.h>
#include <spdlog/spdlog.h>
#include <zmq_addon.hpp>
#include <libbase64.h>
#include <toml.hpp>
#include <3rdparty/secp256k1/include/secp256k1_schnorr.h>
#include <3rdparty/sha2.h>

#include "graphsearch.grpc.pb.h"
#include "utxo.grpc.pb.h"

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
std::atomic<int>           current_block_height = { 210 };
std::atomic<gs::blockhash> current_block_hash(
    std::string("ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff")
);

std::atomic<std::uint64_t> last_incoming_zmq_tx_unix  { 0 };
std::atomic<std::uint64_t> last_outgoing_zmq_tx_unix  { 0 };
std::atomic<gs::txid>      last_incoming_zmq_tx(
    std::string("ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff")
);
std::atomic<gs::txid>      last_outgoing_zmq_tx(
    std::string("ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff")
);

std::atomic<std::uint64_t> last_incoming_zmq_blk_unix { 0 };
std::atomic<std::uint64_t> last_outgoing_zmq_blk_unix { 0 };
std::atomic<std::uint64_t> last_incoming_zmq_blk_size { 0 };
std::atomic<std::uint64_t> last_outgoing_zmq_blk_size { 0 };

std::atomic<bool> utxosync = { false };
std::atomic<bool> exit_early = { false };

boost::shared_mutex processing_mutex;

std::atomic<bool> startup_processing_mempool = { true };
std::vector<gs::transaction> startup_mempool_transactions; // TODO guard with mutex

std::size_t max_exclusion_set_size = 5;
std::array<uint8_t, 32> private_key;
std::atomic<secp256k1_context*> ctx;
boost::filesystem::path cache_dir;

gs::slp_validator validator;
gs::txgraph g;
gs::txgraph mg; // mempool
gs::bch bch;

const std::chrono::milliseconds await_time { 1000 };

std::uint64_t current_time()
{
    auto now = std::chrono::system_clock::now();
    return std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();
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

std::array<uint8_t, 64> schnorr_sign(const std::array<uint8_t, 32>& msg)
{
    std::array<uint8_t, 64> sig = { 0 };
    if (secp256k1_schnorr_sign(ctx, sig.data(), msg.data(), private_key.data(), nullptr, nullptr) != 1) {
        spdlog::warn("schnorr sign failed");
        sig = { 0 };
    }

    return sig;
}

void signal_handler(int signal)
{
    spdlog::info("received signal {} requesting to shut down", signal);

    exit_early = true;

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

        gs::graph_search_status lookup_status;
        size_t lookup_count = 0;

        std::string lookup_txid_str = "";

        // cowardly validating user provided data
        static const std::regex txid_regex("^[0-9a-fA-F]{64}$");
        const bool rmatch = std::regex_match(request->txid(), txid_regex);
        if (rmatch) {
            const gs::txid lookup_txid(request->txid());
            lookup_txid_str = lookup_txid.decompress(true);


            std::vector<gs::txid> exclude_txids;
            for (auto & txid_str : request->exclude_txids()) {
                const bool rmatch = std::regex_match(txid_str, txid_regex);
                if (rmatch) {
                    exclude_txids.emplace_back(txid_str);
                }
            }

            absl::flat_hash_set<const gs::graph_node*> exclusion_set;

            for (const gs::txid & exclusion_txid : exclude_txids) {
                if (! g.build_exclusion_set(exclusion_txid, exclusion_set)) {
                    spdlog::info("build_exclusion_set missing {}", exclusion_txid.decompress(true));
                }

                if (exclude_txids.size() >= max_exclusion_set_size) {
                    break;
                }
            }

            // first check if referring to tx in mempool, this has special handling
            gs::graph_search_response mempool_result = mg.graph_search__ptr(lookup_txid, exclusion_set);
            lookup_status = mempool_result.first;
            lookup_count = mempool_result.second.size();

            if (mempool_result.first == gs::graph_search_status::OK) {

                std::vector<gs::transaction> mempool_transactions;
                mempool_transactions.reserve(mempool_result.second.size());

                absl::flat_hash_set<gs::txid> mempool_txids;

                for (const gs::graph_node & n : mempool_result.second) {
                    for (auto & m : mempool_result.second) {
                        reply->add_txdata(m.data(), m.size());
                    }

                    gs::transaction mtx;
                    mtx.hydrate(n.txdata.begin(), n.txdata.end());
                    mempool_transactions.push_back(mtx);
                    mempool_txids.insert(mtx.txid);
                }

                std::vector<gs::txid> unaccounted_mempool_txids;

                for (const gs::transaction & mtx : mempool_transactions) {
                    for (const gs::outpoint & o : mtx.inputs) {
                        if (! mempool_txids.count(o.txid)) {
                            unaccounted_mempool_txids.push_back(o.txid);
                        }
                    }
                }

                absl::flat_hash_set<const gs::graph_node*> mempool_overlap_set;
                for (const gs::txid & txid : unaccounted_mempool_txids) {
                    gs::graph_search_response result = g.graph_search__ptr(lookup_txid, exclusion_set);
                    g.build_exclusion_set(txid, mempool_overlap_set);
                }

                lookup_count += mempool_overlap_set.size();
                for (const gs::graph_node* n: mempool_overlap_set) {
                    reply->add_txdata(n->txdata.data(), n->txdata.size());
                }
            } else { // txid not in mempool
                gs::graph_search_response result = g.graph_search__ptr(lookup_txid, exclusion_set);
                lookup_status = result.first;
                lookup_count = result.second.size();

                if (lookup_status == gs::graph_search_status::OK) {
                    for (const auto & m : result.second) {
                        reply->add_txdata(m.data(), m.size());
                    }
                }
            }
        } else {
            lookup_txid_str = std::string('*', 64);
        }

        const auto end = std::chrono::steady_clock::now();
        const auto diff = end - start;
        const auto diff_ms = std::chrono::duration<double, std::milli>(diff).count();

        spdlog::info("lookup: {} {} ({} ms)", lookup_txid_str, lookup_count, diff_ms);

        if (! rmatch) {
            return { grpc::StatusCode::INVALID_ARGUMENT, "txid did not match regex" };
        }

        switch (lookup_status) {
            case gs::graph_search_status::OK:
                return { grpc::Status::OK };
            case gs::graph_search_status::NOT_FOUND:
                return { grpc::StatusCode::NOT_FOUND,
                        "txid not found" };
            case gs::graph_search_status::NOT_IN_TOKENGRAPH:
                spdlog::error("graph_search__ptr: txid not found in tokengraph {}", lookup_txid_str);
                return { grpc::StatusCode::INTERNAL,
                        "txid found but not in tokengraph" };
            default:
                spdlog::error("unknown graph_search_status");
                std::exit(EXIT_FAILURE);
        }
    }

    grpc::Status TrustedValidation (
        grpc::ServerContext* context,
        const graphsearch::TrustedValidationRequest* request,
        graphsearch::TrustedValidationReply* reply
    ) override {
        const auto start = std::chrono::steady_clock::now();

        std::string lookup_txid_str = "";

        // cowardly validating user provided data
        static const std::regex txid_regex("^[0-9a-fA-F]{64}$");
        const bool rmatch = std::regex_match(request->txid(), txid_regex);
        if (rmatch) {
            const gs::txid lookup_txid(request->txid());
            lookup_txid_str = lookup_txid.decompress(true);
            const bool valid_tx = validator.has_valid(lookup_txid);
            reply->set_valid(valid_tx);
        }
        const auto end = std::chrono::steady_clock::now();
        const auto diff = end - start;
        const auto diff_ms = std::chrono::duration<double, std::milli>(diff).count();

        spdlog::info("tvalidate: {} ({} ms)", lookup_txid_str, diff_ms);

        if (! rmatch) {
            return { grpc::StatusCode::INVALID_ARGUMENT, "txid did not match regex" };
        }

        return { grpc::Status::OK };
    }

    grpc::Status TrustedValidationBulk (
        grpc::ServerContext* context,
        const graphsearch::TrustedValidationBulkRequest* request,
        graphsearch::TrustedValidationBulkReply* reply
    ) override {
        const auto start = std::chrono::steady_clock::now();

        static const std::regex txid_regex("^[0-9a-fA-F]{64}$");

        std::vector<gs::txid> lookup_txids;
        bool rmatch = true;
        for (auto & rtxid : request->txids()) {
            const std::string txid_str = rtxid.txid();
            rmatch = std::regex_match(txid_str, txid_regex);
            if (! rmatch) {
                break;
            }

            lookup_txids.emplace_back(txid_str);
        }

        if (rmatch) {
            for (auto & lookup_txid : lookup_txids) {
                const bool valid_tx = validator.has_valid(lookup_txid);
                graphsearch::TrustedValidationReply* el = reply->add_valid();
                el->set_valid(valid_tx);
            }
        }

        const auto end = std::chrono::steady_clock::now();
        const auto diff = end - start;
        const auto diff_ms = std::chrono::duration<double, std::milli>(diff).count();

        spdlog::info("tvalidate-bulk: {} ({} ms)", lookup_txids.size(), diff_ms);

        if (! rmatch) {
            return { grpc::StatusCode::INVALID_ARGUMENT, "txid did not match regex" };
        }

        return { grpc::Status::OK };
    }

    grpc::Status OutputOracle (
        grpc::ServerContext* context,
        const graphsearch::OutputOracleRequest* request,
        graphsearch::OutputOracleReply* reply
    ) override {
        const auto start = std::chrono::steady_clock::now();

        std::string lookup_txid_str = "";
        uint32_t lookup_vout = 0;

        // cowardly validating user provided data
        static const std::regex txid_regex("^[0-9a-fA-F]{64}$");
        const bool rmatch = std::regex_match(request->txid(), txid_regex);
        bool valid_tx = false;
        gs::transaction tx;
        if (rmatch) {
            const gs::txid lookup_txid(request->txid());
            lookup_txid_str = lookup_txid.decompress(true);
            valid_tx = validator.has_valid(lookup_txid);
            if (valid_tx) {
                gs::transaction tx = validator.get(lookup_txid);
                lookup_vout = request->vout();

                const gs::txid    txid      = lookup_txid;
                const uint32_t    vout      = lookup_vout;
                const gs::tokenid tokenid   = tx.slp.tokenid;
                const uint16_t    tokentype = tx.slp.token_type;
                const uint64_t    value     = tx.output_slp_amount(vout);

                std::vector<uint8_t> preimage;
                if (tokentype == 0x01) {
                    preimage.resize(32+4+32+2+8+1); // txid, vout, tokenid, tokentype, tokenvalue, is_baton
                    std::memcpy(preimage.data()+0,  txid.data(),    32);
                    std::memcpy(preimage.data()+32, &vout,           4);
                    std::memcpy(preimage.data()+36, tokenid.data(), 32);
                    std::memcpy(preimage.data()+68, &tokentype,      2);
                    std::memcpy(preimage.data()+70, &value,          8);
                    const uint8_t is_baton = tx.mint_baton_outpoint().vout == vout;
                    std::memcpy(preimage.data()+78, &is_baton,       1);
                    // TODO debug, maybe remove in later release
                    reply->set_tx(tx.serialized.data(), tx.serialized.size());
                    reply->set_vout(vout);
                    reply->set_tokenid(tokenid.data(), tokenid.size());
                    reply->set_tokentype(tokentype);
                    reply->set_value(value);
                    reply->set_is_baton(is_baton);
                } else if (tokentype == 0x081) {
                    preimage.resize(32+4+32+2+8+1); // txid, vout, tokenid, tokentype, tokenvalue, is_baton
                    std::memcpy(preimage.data()+0,  txid.data(),    32);
                    std::memcpy(preimage.data()+32, &vout,           4);
                    std::memcpy(preimage.data()+36, tokenid.data(), 32);
                    std::memcpy(preimage.data()+68, &tokentype,      2);
                    std::memcpy(preimage.data()+70, &value,          8);
                    const uint8_t is_baton = tx.mint_baton_outpoint().vout == vout;
                    std::memcpy(preimage.data()+78, &is_baton,  1);
                    // TODO debug, maybe remove in later release
                    reply->set_tx(tx.serialized.data(), tx.serialized.size());
                    reply->set_vout(vout);
                    reply->set_tokenid(tokenid.data(), tokenid.size());
                    reply->set_tokentype(tokentype);
                    reply->set_value(value);
                    reply->set_is_baton(is_baton);
                } else if (tokentype == 0x041) {
                    preimage.resize(32+4+32+2+32); // txid, vout, tokenid, tokentype, 
                    std::memcpy(preimage.data()+0,  txid.data(),    32);
                    std::memcpy(preimage.data()+32, &vout,           4);
                    std::memcpy(preimage.data()+36, tokenid.data(), 32);
                    std::memcpy(preimage.data()+68, &tokentype,      2);
                    // TODO UNTESTED
                    const gs::transaction gen_tx   = validator.get(gs::txid(tokenid.v));
                    const gs::outpoint& i_outpoint = tx.inputs[0];
                    const gs::transaction & txi    = validator.transaction_map.at(i_outpoint.txid);
                    const gs::tokenid group_id     = txi.slp.tokenid;
                    std::memcpy(preimage.data()+70, &group_id,  32);
                    // TODO debug, maybe remove in later release
                    reply->set_tx(tx.serialized.data(), tx.serialized.size());
                    reply->set_vout(vout);
                    reply->set_tokenid(tokenid.data(), tokenid.size());
                    reply->set_tokentype(tokentype);
                    reply->set_groupid(group_id.data(), group_id.size());
                }


                spdlog::info("{} {} {} {}", txid.decompress(true), vout, tokenid.decompress(true), value);
                spdlog::info("{}", gs::util::hex(preimage));

                std::array<uint8_t, 32> msg;
                sha256(preimage.data(), preimage.size(), msg.data());

                const std::array<uint8_t, 64> sig = schnorr_sign(msg);

                reply->set_msg(msg.data(), msg.size());
                reply->set_sig(sig.data(), sig.size());
			}
        }
        const auto end = std::chrono::steady_clock::now();
        const auto diff = end - start;
        const auto diff_ms = std::chrono::duration<double, std::milli>(diff).count();

        spdlog::info("outputoracle: {}:{} ({} ms)", lookup_txid_str, lookup_vout, diff_ms);

        if (! rmatch) {
            return { grpc::StatusCode::INVALID_ARGUMENT, "txid did not match regex" };
        }

        if (! valid_tx) {
            return { grpc::StatusCode::NOT_FOUND, "transaction not found" };
        }

        return { grpc::Status::OK };
    }

    grpc::Status Status (
        grpc::ServerContext* context,
        const graphsearch::StatusRequest* request,
        graphsearch::StatusReply* reply
    ) override {
        reply->set_block_height(current_block_height);
        reply->set_best_block_hash(current_block_hash.load().decompress(true));

        reply->set_last_incoming_zmq_tx_unix (last_incoming_zmq_tx_unix);
        reply->set_last_outgoing_zmq_tx_unix (last_outgoing_zmq_tx_unix);

        reply->set_last_incoming_zmq_tx (last_incoming_zmq_tx.load().decompress(true));
        reply->set_last_outgoing_zmq_tx (last_outgoing_zmq_tx.load().decompress(true));

        reply->set_last_incoming_zmq_blk_unix(last_incoming_zmq_blk_unix);
        reply->set_last_outgoing_zmq_blk_unix(last_outgoing_zmq_blk_unix);

        reply->set_last_incoming_zmq_blk_size(last_incoming_zmq_blk_size);
        reply->set_last_outgoing_zmq_blk_size(last_outgoing_zmq_blk_size);

        return { grpc::Status::OK };
    }

    grpc::Status SlpUtxos(
        grpc::ServerContext* context,
        const graphsearch::SlpUtxosRequest* request,
        graphsearch::SlpUtxosReply* reply
    ) override {
        const auto start = std::chrono::steady_clock::now();

        const gs::scriptpubkey scriptpubkey = gs::scriptpubkey(request->scriptpubkey());
        boost::shared_lock<boost::shared_mutex> lock(bch.lookup_mtx);

        std::vector<gs::output> allUtxos = bch.utxodb.get_outputs_by_scriptpubkey(scriptpubkey, 1e5);

        for (gs::output utxo : allUtxos) {
            auto slp_utxo_search = bch.slpdb.utxo_to_tokenid.find(gs::outpoint(utxo.prev_tx_id, utxo.prev_out_idx));
            if (slp_utxo_search == bch.slpdb.utxo_to_tokenid.end()) {
                continue;
            }
            if (!validator.has(utxo.prev_tx_id)) {
                continue;
            }

            gs::transaction tx = validator.get(utxo.prev_tx_id);
            gs::transaction genesis_tx = validator.get(gs::txid(tx.slp.tokenid.v));
            const gs::slp_transaction_genesis & genesis_info = absl::get<gs::slp_transaction_genesis>(genesis_tx.slp.slp_tx);

            graphsearch::SlpUtxo* el = reply->add_utxos();
            el->set_txid(utxo.prev_tx_id.decompress(true));
            el->set_vout(utxo.prev_out_idx);
            el->set_satoshis(tx.outputs[utxo.prev_out_idx].value);
            el->set_value(tx.output_slp_amount(utxo.prev_out_idx));
            el->set_decimals(genesis_info.decimals);
            el->set_ticker(genesis_info.ticker);
            el->set_tokenid(genesis_tx.txid.decompress(true));
            el->set_type(tx.slp.token_type);
            el->set_isbaton(tx.mint_baton_outpoint().vout == utxo.prev_out_idx);
        }

        const auto end = std::chrono::steady_clock::now();
        const auto diff = end - start;
        const auto diff_ms = std::chrono::duration<double, std::milli>(diff).count();

        spdlog::info("slputxos: {} ({} ms)", scriptpubkey_to_base64(scriptpubkey), diff_ms);
        return { grpc::Status::OK };
    }

    grpc::Status SlpTokenInfo(
        grpc::ServerContext* context,
        const graphsearch::SlpTokenInfoRequest* request,
        graphsearch::SlpTokenInfoReply* reply
    ) override {
        const auto start = std::chrono::steady_clock::now();

        auto binTokenId = gs::util::unhex(request->tokenid());
        std::reverse(binTokenId.begin(), binTokenId.end());
        const gs::tokenid tokenid(binTokenId);

        boost::shared_lock<boost::shared_mutex> lock(bch.lookup_mtx);

        const gs::txid txid(tokenid.v);
        if (!validator.has(txid)) {
            return { grpc::StatusCode::NOT_FOUND, "token " + request->tokenid() + " not found" };
        }

        gs::transaction genesis_tx = validator.get(txid);
        const gs::slp_transaction_genesis & genesis_info = absl::get<gs::slp_transaction_genesis>(genesis_tx.slp.slp_tx);

        reply->set_name(genesis_info.name);
        reply->set_ticker(genesis_info.ticker);
        reply->set_tokenid(genesis_tx.txid.decompress(true));
        reply->set_initialamount(genesis_info.qty);
        reply->set_decimals(genesis_info.decimals);
        reply->set_documenturl(genesis_info.document_uri);
        // std::cout << gs::util::hex2(genesis_info.document_hash) << "\t" << genesis_info.document_hash <<  std::endl;
        reply->set_documenthash(gs::util::hex2(genesis_info.document_hash));
        reply->set_type(genesis_tx.slp.token_type);
        if (genesis_tx.slp.token_type == 0x41) {
            const gs::transaction & txi    = validator.transaction_map.at(genesis_tx.inputs[0].txid);
            const gs::tokenid group_id     = txi.slp.tokenid;

            reply->set_tokenid(group_id.decompress(true));
        }

        const auto end = std::chrono::steady_clock::now();
        const auto diff = end - start;
        const auto diff_ms = std::chrono::duration<double, std::milli>(diff).count();

        spdlog::info("slptokeninfo: {} ({} ms)", request->tokenid(), diff_ms);
        return { grpc::Status::OK };
    }

    grpc::Status SlpTokenBalance(
        grpc::ServerContext* context,
        const graphsearch::SlpTokenBalanceRequest* request,
        graphsearch::SlpTokenBalanceReply* reply
    ) override {
        const auto start = std::chrono::steady_clock::now();

        const gs::scriptpubkey scriptpubkey = gs::scriptpubkey(request->scriptpubkey());
        auto binTokenId = gs::util::unhex(request->tokenid());
        std::reverse(binTokenId.begin(), binTokenId.end());
        const gs::tokenid tokenid(binTokenId);

        boost::shared_lock<boost::shared_mutex> lock(bch.lookup_mtx);

        std::vector<gs::output> allUtxos = bch.utxodb.get_outputs_by_scriptpubkey(scriptpubkey, 1e5);

        std::uint64_t balance = 0;

        for (gs::output utxo : allUtxos) {
            auto slp_utxo_search = bch.slpdb.utxo_to_tokenid.find(gs::outpoint(utxo.prev_tx_id, utxo.prev_out_idx));
            if (slp_utxo_search == bch.slpdb.utxo_to_tokenid.end()) {
                continue;
            }

            gs::transaction tx = validator.get(utxo.prev_tx_id);
            if (tx.slp.tokenid != tokenid) {
                continue;
            }

            balance += tx.output_slp_amount(utxo.prev_out_idx);
        }

        if (balance == 0) {
            reply->set_value(0);
        } else {
            gs::transaction genesis_tx = validator.get(gs::txid(tokenid.v));
            const gs::slp_transaction_genesis & genesis_info = absl::get<gs::slp_transaction_genesis>(genesis_tx.slp.slp_tx);

            reply->set_value(balance);
            reply->set_decimals(genesis_info.decimals);
            reply->set_ticker(genesis_info.ticker);
            reply->set_name(genesis_info.name);
            reply->set_tokenid(genesis_tx.txid.decompress(true));
            reply->set_type(genesis_tx.slp.token_type);
        }

        const auto end = std::chrono::steady_clock::now();
        const auto diff = end - start;
        const auto diff_ms = std::chrono::duration<double, std::milli>(diff).count();

        spdlog::info("slptokenbalance: {} {} ({} ms)", scriptpubkey_to_base64(scriptpubkey), request->tokenid(), diff_ms);
        return { grpc::Status::OK };
    }

    grpc::Status SlpAllTokenBalances(
        grpc::ServerContext* context,
        const graphsearch::SlpAllTokenBalancesRequest* request,
        graphsearch::SlpAllTokenBalancesReply* reply
    ) override {
        const auto start = std::chrono::steady_clock::now();

        const gs::scriptpubkey scriptpubkey = gs::scriptpubkey(request->scriptpubkey());

        boost::shared_lock<boost::shared_mutex> lock(bch.lookup_mtx);

        std::vector<gs::output> allUtxos = bch.utxodb.get_outputs_by_scriptpubkey(scriptpubkey, 1e5);

        absl::flat_hash_map<gs::tokenid, std::uint64_t> balances;

        for (gs::output utxo : allUtxos) {
            auto slp_utxo_search = bch.slpdb.utxo_to_tokenid.find(gs::outpoint(utxo.prev_tx_id, utxo.prev_out_idx));
            if (slp_utxo_search == bch.slpdb.utxo_to_tokenid.end()) {
                continue;
            }

            gs::transaction tx = validator.get(utxo.prev_tx_id);

            balances[tx.slp.tokenid] += tx.output_slp_amount(utxo.prev_out_idx);
        }

        for (const auto & pair : balances) {
            gs::transaction genesis_tx = validator.get(gs::txid(pair.first.v));
            const gs::slp_transaction_genesis & genesis_info = absl::get<gs::slp_transaction_genesis>(genesis_tx.slp.slp_tx);

            graphsearch::SlpTokenBalanceReply* el = reply->add_balances();

            el->set_value(pair.second);
            el->set_decimals(genesis_info.decimals);
            el->set_ticker(genesis_info.ticker);
            el->set_name(genesis_info.name);
            el->set_tokenid(genesis_tx.txid.decompress(true));
            el->set_type(genesis_tx.slp.token_type);
        }

        const auto end = std::chrono::steady_clock::now();
        const auto diff = end - start;
        const auto diff_ms = std::chrono::duration<double, std::milli>(diff).count();

        spdlog::info("slpalltokenbalances: {} ({} ms)", scriptpubkey_to_base64(scriptpubkey), diff_ms);
        return { grpc::Status::OK };
    }
};

class UtxoServiceImpl final
 : public graphsearch::UtxoService::Service
{
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

bool slpsync_bitcoind_process_block(const gs::block& block, const bool mempool, const bool trusted, std::vector<gs::transaction> * valid_txs_list = nullptr)
{
    boost::lock_guard<boost::shared_mutex> lock(processing_mutex);

    absl::flat_hash_map<gs::tokenid, std::vector<gs::transaction>> valid_txs;
    for (auto & tx : block.txs) {
        if (validator.has(tx.txid)) {
            // skip over ones we've already added from mempool // TODO correct doc?
            continue;
        }
        if (! validator.add_tx(tx, trusted)) {
            std::cerr << "invalid tx: " << tx.txid.decompress(true) << std::endl;
            continue;
        }

        if (! valid_txs.count(tx.slp.tokenid)) {
            valid_txs.insert({ tx.slp.tokenid, { tx } });
        } else {
            valid_txs[tx.slp.tokenid].push_back(tx);
        }

        if (valid_txs_list) {
            valid_txs_list->push_back(tx);
        }
    }

    for (auto & m : valid_txs) {
        g.insert_token_data(m.first, m.second);
    }

    if (utxosync) {
        bch.process_block(block, true);
    }

    spdlog::info("processed block {} ({}) [{}/{}]", current_block_height, validator.valid.size(), valid_txs.size(), block.txs.size());

    return true;
}

bool slpsync_bitcoind_process_mempool(const gs::block& block)
{
    boost::lock_guard<boost::shared_mutex> lock(processing_mutex);
    mg.clear();

    absl::flat_hash_map<gs::tokenid, std::vector<gs::transaction>> valid_txs;
    for (auto & tx : block.txs) {
        if (validator.has(tx.txid)) {
            // skip over ones we've already added from mempool
            continue;
        }
        if (! validator.add_tx(tx, false)) {
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
        mg.insert_token_data(m.first, m.second);
    }

    if (utxosync) {
        for (const auto & token_tx_vec : valid_txs) {
            for (const auto & tx : token_tx_vec.second) {
                bch.process_mempool_tx(tx);
            }
        }
    }

    spdlog::info("processed mempool ({}) [{}]", validator.valid.size(), block.txs.size());

    return true;
}

bool slpsync_bitcoind_process_tx(const gs::transaction& tx)
{
    boost::lock_guard<boost::shared_mutex> lock(processing_mutex);

    spdlog::info("zmq-tx {}", tx.txid.decompress(true));

    if (tx.slp.type == gs::slp_transaction_type::invalid) {
        // spdlog::warn("zmq-tx invalid {}", tx.txid.decompress(true));
        return false;
    }

    if (validator.has(tx.txid)) {
        spdlog::warn("zmq-tx already in validator {}", tx.txid.decompress(true));
        return false;
    }

    if (! validator.add_tx(tx, false)) {
        spdlog::warn("zmq-tx invalid tx: {}", tx.txid.decompress(true));
        return false;
    }

    mg.insert_token_data(tx.slp.tokenid, { tx });

    if (utxosync) {
        bch.process_mempool_tx(tx);
    }

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

    auto serialized = block.serialize();
    outf.write(reinterpret_cast<const char *>(serialized.data()), serialized.size());

    return true;
}

std::vector<gs::transaction> get_mempool_transactions(gs::rpc & rpc)
{
    std::vector<gs::transaction> ret;

    const std::pair<bool, std::vector<gs::txid>> txids = rpc.get_raw_mempool();
    if (! txids.first) {
        throw std::runtime_error("get_raw_mempool failed");
    }

    for (const gs::txid & txid : txids.second) {
        const std::pair<bool, std::vector<std::uint8_t>> txdata = rpc.get_raw_transaction(txid);

        if (! txdata.first) {
            throw std::runtime_error("get_raw_transaction failed");
        }

        gs::transaction tx;
        const bool hydration_success = tx.hydrate(txdata.second.begin(), txdata.second.end());

        if (! hydration_success) {
            throw std::runtime_error("failed to hydrate mempool tx");
        }

        if (tx.slp.type != gs::slp_transaction_type::invalid) {
            ret.push_back(tx);
        }
    }

    return ret;
}

int main(int argc, char * argv[])
{
    // std::signal(SIGINT, signal_handler);
    // std::signal(SIGTERM, signal_handler);
    startup_mempool_transactions.reserve(100000);

    if (argc < 2) {
        std::cerr << "usage: gs++ config.toml\n";
        return EXIT_FAILURE;
    }

    const auto config = toml::parse(argv[1]);
    const bool cache_enabled = toml::find<bool>(config, "services", "cache");
    if (cache_enabled) {
        cache_dir = boost::filesystem::path(toml::find<std::string>(config, "cache", "dir"));
    }
    max_exclusion_set_size = toml::find<std::size_t>(config, "graphsearch", "max_exclusion_set_size");
    {
        const std::vector<uint8_t> privkey = gs::util::unhex(
            toml::find<std::string>(config, "graphsearch", "private_key")
        );

        if (privkey.size() != 32) {
            spdlog::error("private_key has bad format");
            return EXIT_FAILURE;
        }

        std::memcpy(private_key.data(), privkey.data(), 32);

        ctx = secp256k1_context_create(SECP256K1_CONTEXT_SIGN);
    }

    spdlog::info("hello");

    gs::rpc rpc(
        toml::find<std::string>  (config, "bitcoind", "host"),
        toml::find<std::uint16_t>(config, "bitcoind", "port"),
        toml::find<std::string>  (config, "bitcoind", "user"),
        toml::find<std::string>  (config, "bitcoind", "pass")
    );

    if (toml::find<bool>(config, "services", "utxosync")) {
        utxosync = true;
        if (toml::find<bool>(config, "utxo", "checkpoint_load")) {
        }

        const std::pair<bool, std::uint32_t> best_block_height = rpc.get_best_block_height();
        if (! best_block_height.first) {
            spdlog::error("could not connect to rpc");
            return EXIT_FAILURE;
        }

        spdlog::info("best block height: {}", best_block_height.second);
        for (
            std::uint32_t block_height=toml::find<std::uint32_t>(config, "utxo", "block_height");
            block_height <= best_block_height.second;
            ++block_height
        ) {
            const std::pair<bool, gs::blockhash> block_hash = rpc.get_block_hash(block_height);
            if (! block_hash.first) {
                spdlog::warn("rpc request failed, trying again...");
                std::this_thread::sleep_for(await_time);
                --block_height;
                continue;
            }

            const std::pair<bool, std::vector<std::uint8_t>> block_data = rpc.get_raw_block(block_hash.second);
            if (! block_data.first) {
                spdlog::warn("rpc request failed, trying again...");
                std::this_thread::sleep_for(await_time);
                --block_height;
                continue;
            }

            spdlog::info("processing block {}", block_height);
            bch.process_block(block_data.second, true);
        }

        if (toml::find<bool>(config, "utxo", "checkpoint_save")) {
        }
    }

    if (toml::find<bool>(config, "services", "graphsearch")) {
        if (cache_enabled) {
            for (; ! exit_early; ++current_block_height) {
                boost::filesystem::path blk_path = block_height_to_path(current_block_height) / std::to_string(current_block_height);
                if (! boost::filesystem::exists(blk_path)) {
                    --current_block_height;
                    break;
                }

                std::ifstream ifs(blk_path.string(), std::ios::in | std::ios::binary);
                const std::vector<std::uint8_t> blk_data((std::istreambuf_iterator<char>(ifs)),
                                                          std::istreambuf_iterator<char>());

                gs::block block;
                if (! block.hydrate(blk_data.begin(), blk_data.end())) {
                    spdlog::error("failed to hydrate cache block {}", current_block_height);
                    --current_block_height;
                    return 0; // TODO Delete me
                    break;
                }

                current_block_hash = block.block_hash;

                if (! slpsync_bitcoind_process_block(block, false, true)) {
                    spdlog::error("failed to process cache block {}", current_block_height);
                    --current_block_height;
                    break;
                }
            }
        }
        if (toml::find<bool>(config, "services", "graphsearch_rpc")) {
            while (! exit_early) {
    retry_loop2:
                const std::pair<bool, std::uint32_t> best_block_height = rpc.get_best_block_height();
                if (! best_block_height.first) {
                    spdlog::error("could not connect to rpc");
                    return EXIT_FAILURE;
                }

                spdlog::info("best block height: {}", best_block_height.second);

                if (current_block_height == best_block_height.second) {
                    break;
                }

                for (;
                    ! exit_early && current_block_height <= best_block_height.second;
                    ++current_block_height
                ) {
                    const std::pair<bool, gs::blockhash> block_hash = rpc.get_block_hash(current_block_height);
                    if (! block_hash.first) {
                        spdlog::warn("rpc request failed, trying again...");
                        std::this_thread::sleep_for(await_time);
                        --current_block_height;
                        goto retry_loop2;
                    }

                    const std::pair<bool, std::vector<std::uint8_t>> block_data = rpc.get_raw_block(block_hash.second);
                    if (! block_data.first) {
                        spdlog::warn("rpc request failed, trying again...");
                        std::this_thread::sleep_for(await_time);
                        --current_block_height;
                        goto retry_loop2;
                    }

                    gs::block block;
                    if (! block.hydrate(block_data.second.begin(), block_data.second.end(), true)) {
                        spdlog::error("failed to hydrate rpc block {}", current_block_height);
                        std::this_thread::sleep_for(await_time);
                        --current_block_height;
                        goto retry_loop2;
                    }

                    current_block_hash = block.block_hash;

                    block.topological_sort();
                    std::vector<gs::transaction> valid_txs;
                    if (! slpsync_bitcoind_process_block(block, false, false, &valid_txs)) {
                        spdlog::error("failed to process rpc block {}", current_block_height);
                        std::this_thread::sleep_for(await_time);
                        --current_block_height;
                        goto retry_loop2;
                    }

                    current_block_hash = block_hash.second;

                    if (cache_enabled) {
                        gs::block cblk = block;
                        cblk.txs = valid_txs;
                        cblk.topological_sort();
                        cache_slp_block(cblk, current_block_height);
                    }
                } --current_block_height;

                break;
            }
        }
    }

    std::thread zmq_listener([&] {
        if (! toml::find<bool>(config, "services", "zmq")) {
            return;
        }
        zmq::context_t subcontext(1);
        zmq::socket_t subsock(subcontext, zmq::socket_type::sub);
        subsock.connect(
            "tcp://"+
            toml::find<std::string>(config, "bitcoind", "host")+
            ":"+
            std::to_string(toml::find<std::uint16_t>(config, "bitcoind", "zmq_port"))
        );
        subsock.setsockopt(ZMQ_SUBSCRIBE, "", 0);


        zmq::context_t pubcontext;
        zmq::socket_t pubsock(pubcontext, zmq::socket_type::pub);

        const bool zmqpub = toml::find<bool>(config, "services", "zmqpub");
        if (zmqpub) {
            pubsock.bind(toml::find<std::string>(config, "zmqpub", "bind"));
        }

        while (! exit_early) {
            try {
                zmq::message_t env;
                subsock.recv(&env);
                const std::string env_str(static_cast<char*>(env.data()), env.size());

                if (env_str == "rawtx" || env_str == "rawblock") {
                    // std::cout << "Received envelope '" << env_str << "'" << std::endl;

                    zmq::message_t msg;
                    subsock.recv(&msg);

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
                            gs::transaction tx;
                            if (! tx.hydrate(msg_data.begin(), msg_data.end())) {
                                spdlog::error("zmq-tx unable to be hydrated");
                                continue;
                            }
                            last_incoming_zmq_tx      = tx.txid;
                            last_incoming_zmq_tx_unix = current_time();

                            if (! slpsync_bitcoind_process_tx(tx)) {
                                // spdlog::warn("failed to process zmq tx {}", tx.txid.decompress(true));
                                continue;
                            }
                            if (zmqpub) {
                                spdlog::info("publishing zmq tx {}", tx.txid.decompress(true));
                                std::array<zmq::const_buffer, 2> msgs = {
                                    zmq::str_buffer("rawtx"),
                                    zmq::buffer(tx.serialized.data(), tx.serialized.size())
                                };
                                zmq::send_multipart(pubsock, msgs, zmq::send_flags::dontwait);

                                last_outgoing_zmq_tx      = tx.txid;
                                last_outgoing_zmq_tx_unix = current_time();
                            }
                        }
                        if (env_str == "rawblock") {
                            gs::block block;
                            if (! block.hydrate(msg_data.begin(), msg_data.end(), true)) {
                                spdlog::error("failed to hydrate zmq block");
                                continue;
                            }
                            last_incoming_zmq_blk_size = block.txs.size();
                            last_incoming_zmq_blk_unix = current_time();

                            block.topological_sort();

                            ++current_block_height;
                            if (! slpsync_bitcoind_process_block(block, false, false)) {
                                spdlog::error("failed to process zmq block {}", current_block_height);
                                --current_block_height;
                                continue;
                            }
                            std::vector<gs::transaction> mempool_transactions = get_mempool_transactions(rpc);
                            gs::block mempool_block;
                            mempool_block.txs = mempool_transactions;
                            mempool_block.topological_sort();
                            slpsync_bitcoind_process_mempool(mempool_block);


                            current_block_hash = block.block_hash;

                            if (zmqpub) {
                                spdlog::info("publishing zmq block {}", block.merkle_root.decompress(true));

                                const std::vector<std::uint8_t> bserial = block.serialize();
                                std::array<zmq::const_buffer, 2> msgs = {
                                    zmq::str_buffer("rawblock"),
                                    zmq::buffer(bserial.data(), bserial.size())
                                };
                                zmq::send_multipart(pubsock, msgs, zmq::send_flags::dontwait);

                                last_outgoing_zmq_blk_size = block.txs.size();
                                last_outgoing_zmq_blk_unix = current_time();
                            }
                        }
                    }
                }
            } catch (const zmq::error_t& e) {
                spdlog::error(e.what());
            }
        }
    });

    if (toml::find<bool>(config, "services", "graphsearch")
     && toml::find<bool>(config, "services", "graphsearch_rpc"))
    {
        while (true) {
retry_loop1:
            if (exit_early) break;

            try {
                const auto mempool_transactions = get_mempool_transactions(rpc);
                for (const gs::transaction & tx : mempool_transactions) {
                    startup_mempool_transactions.push_back(tx);
                }
                break;
            } catch (std::runtime_error e) {
                spdlog::error(e.what());
                std::this_thread::sleep_for(await_time);
                goto retry_loop1;
            }
        }
    }


    // repurposing to use as mempool container
    gs::block mempool_block;
    mempool_block.txs = startup_mempool_transactions;
    mempool_block.topological_sort();
    slpsync_bitcoind_process_mempool(mempool_block);
    startup_processing_mempool = false;


    if (! exit_early && toml::find<bool>(config, "services", "grpc")) {
        const std::string server_address(
            toml::find<std::string>(config, "grpc", "host")+
            ":"+
            std::to_string(toml::find<std::uint16_t>(config, "grpc", "port"))
        );


        GraphSearchServiceImpl graphsearch_service;
        UtxoServiceImpl utxo_service;
        grpc::ServerBuilder builder;
        builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
        builder.RegisterService(&graphsearch_service);
        builder.RegisterService(&utxo_service);
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
