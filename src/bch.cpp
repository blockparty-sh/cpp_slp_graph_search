#include <vector>
#include <cstdint>
#include <algorithm>
#include <shared_mutex>
#include <stack>
#include <functional>

#include <spdlog/spdlog.h>
#include <absl/container/flat_hash_map.h>
#include <absl/container/flat_hash_set.h>
#include <gs++/bch.hpp>
#include <gs++/transaction.hpp>
#include <gs++/slp_transaction.hpp>

namespace gs {

void topological_sort_internal(
	const gs::transaction& tx,
    const absl::flat_hash_map<gs::txid, gs::transaction> & transactions,
    std::vector<gs::txid> & stack,
    absl::flat_hash_set<gs::txid> & visited
) {
    visited.insert(tx.txid);

    for (const gs::outpoint & outpoint : tx.inputs) {
        if (visited.count(outpoint.txid)      == 0
        &&  transactions.count(outpoint.txid) == 1
        ) {
            topological_sort_internal(
				transactions.at(outpoint.txid),
                transactions,
                stack,
                visited
			);
        }
    }
    stack.push_back(tx.txid);
}

std::vector<gs::transaction> bch::topological_sort(
    const std::vector<gs::transaction>& tx_list
) {
    absl::flat_hash_map<gs::txid, gs::transaction> transactions;
    for (auto & tx : tx_list) {
        transactions.insert({ tx.txid, tx });
    }

    std::vector<gs::txid> stack;
    absl::flat_hash_set<gs::txid> visited;

    for (auto & tx : tx_list) {
        if (visited.count(tx.txid) == 0) {
            topological_sort_internal(tx, transactions, stack, visited);
        }
    }

    std::vector<gs::transaction> ret;
    ret.reserve(stack.size());
    for (auto & txid : stack) {
        ret.emplace_back(transactions[txid]);
    }

    return ret;
}

// TODO we need to check for memory leaks/bugs
// they most likely would exist in this function 
// especially around the mempool utxo set
void bch::process_block(
    const std::vector<std::uint8_t>& block_data,
    const bool save_rollback
) {
    std::lock_guard lock(lookup_mtx);

    ++utxodb.current_block_height;

    auto it = block_data.begin();
    const std::uint32_t version { gs::util::extract_u32(it) };

    gs::txid prev_block; // TODO detect re-org
    std::copy(it, it+32, reinterpret_cast<char*>(prev_block.begin()));
    it+=32;

    gs::txid merkle_root;
    std::copy(it, it+32, reinterpret_cast<char*>(merkle_root.begin()));
    it+=32;

    const std::uint32_t timestamp { gs::util::extract_u32(it) };
    const std::uint32_t bits      { gs::util::extract_u32(it) };
    const std::uint32_t nonce     { gs::util::extract_u32(it) };
    const std::uint64_t txn_count { gs::util::extract_var_int(it) };


    /*
    std::cout
        << version << "\n"
        << prev_block.decompress(true) << "\n"
        << merkle_root.decompress(true) << "\n"
        << timestamp << "\n"
        << bits << "\n"
        << nonce << "\n"
        << txn_count << "\n";
    */

    std::vector<gs::outpoint>    blk_inputs;
    std::vector<gs::output>      blk_outputs;
    std::vector<gs::transaction> slp_txs;

    std::size_t total_added = 0;
    std::size_t total_removed = 0;

    for (std::uint64_t i=0; i<txn_count; ++i) {
        gs::transaction tx(it, utxodb.current_block_height);

        for (auto & m : tx.inputs) {
            blk_inputs.emplace_back(m);
        }

        for (auto & m : tx.outputs) {
            blk_outputs.emplace_back(m);
        }

        if (tx.slp.type != gs::slp_transaction_type::invalid) {
            slp_txs.emplace_back(tx);
        }
    }

    // only used if save_rollback enabled
    std::vector<gs::outpoint> this_block_added;
    std::vector<gs::output>   this_block_removed;

    for (auto & m : blk_outputs) {
        if (m.is_op_return()) {
            continue;
        }

        const gs::outpoint outpoint(m.prev_tx_id, m.prev_out_idx);
        gs::output* const oid = &(*utxodb.outpoint_map.insert({ outpoint, m }).first).second;
        ++total_added;

        if (! utxodb.scriptpubkey_to_output.count(m.scriptpubkey)) {
            utxodb.scriptpubkey_to_output.insert({ m.scriptpubkey, { oid } });
        } else {
            utxodb.scriptpubkey_to_output[m.scriptpubkey].emplace(oid);
        }

        utxodb.mempool_outpoint_map.erase(outpoint);
        if (utxodb.mempool_scriptpubkey_to_output.count(m.scriptpubkey) > 0) {
            utxodb.mempool_scriptpubkey_to_output.at(m.scriptpubkey).erase(oid);
        }
        utxodb.mempool_spent_confirmed_outpoints.erase(gs::outpoint(oid->prev_tx_id, oid->prev_out_idx));

        if (save_rollback) {
            this_block_added.push_back(outpoint);
        }
        // std::cout << height << "\tadded: " << m.prev_tx_id.decompress(true) << ":" << m.prev_out_idx << "\n";
    }

    slp_txs = topological_sort(slp_txs);
    for (auto & m : slp_txs) {
        slpdb.add_transaction(m);
    }

    for (auto & m : blk_inputs) {
        if (utxodb.outpoint_map.count(m) > 0) {
            const gs::output& o = utxodb.outpoint_map.at(m);

            if (utxodb.scriptpubkey_to_output.count(o.scriptpubkey) > 0) {
                absl::flat_hash_set<gs::output*> & addr_map = utxodb.scriptpubkey_to_output.at(o.scriptpubkey);

                if (addr_map.erase(&o)) {
                    // std::cout << height << "\tremoved: " << m.txid.decompress(true) << ":" << m.vout << "\n";
                }
                if (addr_map.empty()) {
                    utxodb.scriptpubkey_to_output.erase(o.scriptpubkey);
                }

                utxodb.mempool_spent_confirmed_outpoints.erase(gs::outpoint(o.prev_tx_id, o.prev_out_idx));
            }

            if (save_rollback) {
                this_block_removed.push_back(o);
            }

            if (utxodb.outpoint_map.erase(m)) {
                ++total_removed;
            }
        }

        if (utxodb.mempool_outpoint_map.count(m) > 0) {
            const gs::output& o = utxodb.mempool_outpoint_map.at(m);

            if (utxodb.mempool_scriptpubkey_to_output.count(o.scriptpubkey) > 0) {
                absl::flat_hash_set<gs::output*> & addr_map = utxodb.mempool_scriptpubkey_to_output.at(o.scriptpubkey);

                if (addr_map.erase(&o)) {
                    // std::cout << height << "\tremoved: " << m.txid.decompress(true) << ":" << m.vout << "\n";
                }
                if (addr_map.empty()) {
                    utxodb.mempool_scriptpubkey_to_output.erase(o.scriptpubkey);
                }
            }

            if (save_rollback) {
                this_block_removed.push_back(o);
            }

            if (utxodb.mempool_outpoint_map.erase(m)) {
                ++total_removed;
            }
        }


        // remove slp utxos
        auto slp_utxo_to_tokenid_search = slpdb.utxo_to_tokenid.find(m);
        if (slp_utxo_to_tokenid_search == slpdb.utxo_to_tokenid.end()) {
            continue;
        }

        gs::slp_token & slp_token = slpdb.tokens[slp_utxo_to_tokenid_search->second];
        if (! slp_token.utxos.erase(m)) {
            spdlog::error("deleted slp utxo not found");
        } else if (slp_token.mint_baton_outpoint.has_value()
               &&  slp_token.mint_baton_outpoint == m
        ) {
            slp_token.mint_baton_outpoint.reset();
        }
    }

    if (save_rollback) {
        utxodb.last_block_added.push_back(this_block_added);
        utxodb.last_block_removed.push_back(this_block_removed);

        if (utxodb.last_block_added.size() > utxodb.rollback_depth) {
            utxodb.last_block_added.pop_front();
        }
        if (utxodb.last_block_removed.size() > utxodb.rollback_depth) {
            utxodb.last_block_removed.pop_front();
        }
    }

    // spdlog::info("processed block +{} -{}", total_added, total_removed);
}

void bch::process_mempool_tx(const std::vector<std::uint8_t>& msg_data)
{
    std::lock_guard lock(lookup_mtx);

    gs::transaction tx(msg_data.begin(), 0);
    spdlog::info("processing tx {}", tx.txid.decompress(true));

    // std::cout << "txid: " << tx.txid.decompress(true) << std::endl;
    // std::cout << "\tversion: " << tx.version << std::endl;
    // std::cout << "\tlock_time: " << tx.lock_time << std::endl;

    for (auto & m : tx.outputs) {
        // std::cout << "\toutput txid: " << m.prev_tx_id.decompress(true) << "\t" << m.prev_out_idx << std::endl; 
        if (m.is_op_return()) {
            continue;
        }

        const gs::outpoint outpoint(m.prev_tx_id, m.prev_out_idx);
        gs::output* const oid = &(*utxodb.mempool_outpoint_map.insert({ outpoint, m }).first).second;

        if (! utxodb.scriptpubkey_to_output.count(m.scriptpubkey)) {
            utxodb.mempool_scriptpubkey_to_output.insert({ m.scriptpubkey, { oid } });
        } else {
            utxodb.mempool_scriptpubkey_to_output[m.scriptpubkey].emplace(oid);
        }
    }

    for (auto & m : tx.inputs) {
        // std::cout << "\tinput txid: " << m.txid.decompress(true) << ":" << m.vout << "\n";

        if (utxodb.mempool_outpoint_map.count(m) == 0) {
            utxodb.mempool_spent_confirmed_outpoints.insert(m);
            continue;
        }

        const gs::output& o = utxodb.mempool_outpoint_map.at(m);

        if (! utxodb.mempool_scriptpubkey_to_output.count(o.scriptpubkey)) {
            continue;
        }
        absl::flat_hash_set<gs::output*> & addr_map = utxodb.mempool_scriptpubkey_to_output.at(o.scriptpubkey);
        if (addr_map.erase(&o)) {
            // std::cout << height << "\tremoved: " << m.txid.decompress(true) << ":" << m.vout << "\n";
        }
        if (addr_map.empty()) {
            utxodb.mempool_scriptpubkey_to_output.erase(o.scriptpubkey);
        }
    }

    if (tx.slp.type != gs::slp_transaction_type::invalid) {
        slpdb.add_transaction(tx);
    }
}




void bch::rollback()
{}

}