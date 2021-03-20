#include <vector>
#include <cstdint>
#include <algorithm>
#include <stack>
#include <functional>
#include <cassert>

#include <boost/thread.hpp>
#include <spdlog/spdlog.h>
#include <absl/container/flat_hash_map.h>
#include <absl/container/flat_hash_set.h>
#include <gs++/bch.hpp>
#include <gs++/transaction.hpp>
#include <gs++/slp_transaction.hpp>
#include <gs++/block.hpp>

// TODO merge this into gs++ server eventually when working

namespace gs {

// TODO we need to check for memory leaks/bugs
// they most likely would exist in this function 
// especially around the mempool utxo set
void bch::process_block(
    const std::vector<std::uint8_t>& block_data,
    const bool save_rollback
) {
    gs::block block;
    block.hydrate(block_data.begin(), block_data.end());

    process_block(block, save_rollback);
}

void bch::process_block(
    const gs::block & block,
    const bool save_rollback
) {
    boost::lock_guard<boost::shared_mutex> lock(lookup_mtx);

    ++utxodb.current_block_height;

    std::vector<gs::transaction> slp_txs;

    std::size_t total_added = 0;
    std::size_t total_removed = 0;

    // only used if save_rollback enabled
    std::vector<gs::outpoint> this_block_added;
    std::vector<gs::output>   this_block_removed;

    for (auto & tx : block.txs) {
        if (tx.slp.type != gs::slp_transaction_type::invalid) {
            slp_txs.push_back(tx);
        }
    }

    slp_txs = gs::util::topological_sort(slp_txs);
    for (auto & m : slp_txs) {
        slpdb.add_transaction(m);
    }

    for (auto & tx : slp_txs) {
        // process inputs
        for (auto & m : tx.inputs) {
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

            assert(slpdb.tokens.count(slp_utxo_to_tokenid_search->second) == 1);
            gs::slp_token & slp_token = slpdb.tokens[slp_utxo_to_tokenid_search->second];
            if (! slp_token.utxos.erase(m)) {
                spdlog::error("deleted slp utxo not found");
            } else if (slp_token.mint_baton_outpoint.has_value()
                &&  slp_token.mint_baton_outpoint == m
            ) {
                slp_token.mint_baton_outpoint.reset();
            }
        }

        // process outputs
        for (auto & m : tx.slp_outputs()) {
            if (m.is_op_return()) {
                continue;
            }

            const gs::outpoint outpoint(m.prev_tx_id, m.prev_out_idx);
            gs::output* const oid = &(*utxodb.outpoint_map.insert({ outpoint, m }).first).second;
            ++total_added;

            if (! utxodb.scriptpubkey_to_output.count(m.scriptpubkey)) {
                utxodb.scriptpubkey_to_output.insert({ m.scriptpubkey, { oid } });
            } else {
                utxodb.scriptpubkey_to_output[m.scriptpubkey].insert(oid);
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
    gs::transaction tx;
    const bool hydration_success = tx.hydrate(msg_data.begin(), msg_data.end());
    assert(hydration_success);

    process_mempool_tx(tx);
}

void bch::process_mempool_tx(const gs::transaction& tx)
{
    if (tx.slp.type == gs::slp_transaction_type::invalid) {
        return;
    }

    boost::lock_guard<boost::shared_mutex> lock(lookup_mtx);

    spdlog::info("processing tx {}", tx.txid.decompress(true));

    slpdb.add_transaction(tx);

    // std::cout << "txid: " << tx.txid.decompress(true) << std::endl;
    // std::cout << "\tversion: " << tx.version << std::endl;
    // std::cout << "\tlock_time: " << tx.lock_time << std::endl;

    for (auto & m : tx.slp_outputs()) {
        // std::cout << "\toutput txid: " << m.prev_tx_id.decompress(true) << "\t" << m.prev_out_idx << std::endl; 
        if (m.is_op_return()) {
            continue;
        }

        const gs::outpoint outpoint(m.prev_tx_id, m.prev_out_idx);
        gs::output* const oid = &(*utxodb.mempool_outpoint_map.insert({ outpoint, m }).first).second;
        if (! utxodb.mempool_scriptpubkey_to_output.count(m.scriptpubkey)) {
            utxodb.mempool_scriptpubkey_to_output.insert({ m.scriptpubkey, { oid } });
        } else {
            utxodb.mempool_scriptpubkey_to_output[m.scriptpubkey].insert(oid);
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
}




void bch::rollback()
{}

}
