#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <algorithm>
#include <iostream>
#include <string>
#include <vector>
#include <cstdint>
#include <fstream>
#include <variant>
#include <memory>

#include <boost/thread.hpp>
#include <absl/container/flat_hash_map.h>
#include <absl/container/node_hash_map.h>
#include <absl/container/flat_hash_set.h>

#include <spdlog/spdlog.h>

#include <gs++/bhash.hpp>
#include <gs++/output.hpp>
#include <gs++/scriptpubkey.hpp>
#include <gs++/util.hpp>
#include <gs++/transaction.hpp>
#include <gs++/utxodb.hpp>



namespace gs {

utxodb::utxodb()
: current_block_height(0)
, current_block_hash("0000000000000000000000000000000000000000000000000000000000000000")
{}

void utxodb::rollback()
{
    boost::lock_guard<boost::shared_mutex> lock(lookup_mtx);

    if (last_block_removed.empty() || last_block_added.empty()) {
        return;
    }

    const std::vector<gs::output> rb_removed = last_block_removed.back();
    const std::vector<gs::outpoint> rb_added = last_block_added.back();
    last_block_removed.pop_back();
    last_block_added.pop_back();

    for (auto m : rb_removed) {
        gs::output* const oid = &(*outpoint_map.insert({
            gs::outpoint(m.prev_tx_id, m.prev_out_idx),
            m
        }).first).second;

        if (! scriptpubkey_to_output.count(m.scriptpubkey)) {
            scriptpubkey_to_output.insert({ m.scriptpubkey, { oid } });
        } else {
            scriptpubkey_to_output[m.scriptpubkey].insert(oid);
        }

        // std::cout << "\tadded: " << m.prev_tx_id.decompress(true) << ":" << m.prev_out_idx << "\n";
    }

    for (auto m : rb_added) {
        if (outpoint_map.count(m) == 0) {
            continue;
        }

        const gs::output& o                 = outpoint_map.at(m);
        const gs::scriptpubkey scriptpubkey = o.scriptpubkey;

        if (! scriptpubkey_to_output.count(scriptpubkey)) {
            continue;
        }
        absl::flat_hash_set<gs::output*> & addr_map = scriptpubkey_to_output.at(scriptpubkey);
        if (addr_map.erase(&o)) {
            // std::cout << "\tremoved: " << m.txid.decompress(true) << ":" << m.vout << "\n";
            if (addr_map.empty()) {
                scriptpubkey_to_output.erase(scriptpubkey);
            }
        }
    }
}


std::vector<gs::output> utxodb::get_outputs_by_outpoints(
    const std::vector<gs::outpoint> outpoints
) {
    boost::shared_lock<boost::shared_mutex> lock(lookup_mtx);

    std::vector<output> ret;
    ret.reserve(outpoints.size()); // most of the time this will be same size

    for (auto o : outpoints) {
        const auto outpoint_map_search = outpoint_map.find(o);
        if (outpoint_map_search != outpoint_map.end()) {
            if (mempool_spent_confirmed_outpoints.count(o) == 0) {
                ret.push_back(outpoint_map_search->second);
            }
        } else {
            const auto mempool_outpoint_map_search = mempool_outpoint_map.find(o);
            if (mempool_outpoint_map_search != mempool_outpoint_map.end()) {
                ret.push_back(mempool_outpoint_map_search->second);
            }
        }
    }

    return ret;
}

std::vector<gs::output> utxodb::get_outputs_by_scriptpubkey(
    const gs::scriptpubkey scriptpubkey,
    const std::uint32_t limit
) {
    boost::shared_lock<boost::shared_mutex> lock(lookup_mtx);

    std::vector<gs::output> ret;

    if (scriptpubkey_to_output.count(scriptpubkey) > 0) {
        const absl::flat_hash_set<gs::output*>& pk_utxos = scriptpubkey_to_output.at(scriptpubkey);

        for (const gs::output* u : pk_utxos) {
            if (mempool_spent_confirmed_outpoints.count(gs::outpoint(u->prev_tx_id, u->prev_out_idx)) == 0) {
                ret.push_back(*u);
                if (ret.size() == limit) {
                    break;
                }
            }
        }
    }

    if (ret.size() < limit && mempool_scriptpubkey_to_output.count(scriptpubkey) > 0) {
        const absl::flat_hash_set<gs::output*>& pk_utxos = mempool_scriptpubkey_to_output.at(scriptpubkey);

        for (const gs::output* u : pk_utxos) {
            ret.push_back(*u);
            if (ret.size() == limit) {
                break;
            }
        }
    }

    return ret;
}

std::uint64_t utxodb::get_balance_by_scriptpubkey(
    const gs::scriptpubkey scriptpubkey
) {
    boost::shared_lock<boost::shared_mutex> lock(lookup_mtx);

    std::uint64_t ret = 0;

    if (scriptpubkey_to_output.count(scriptpubkey) > 0) {
        const absl::flat_hash_set<gs::output*>& pk_utxos = scriptpubkey_to_output.at(scriptpubkey);

        for (const gs::output* u : pk_utxos) {
            if (mempool_spent_confirmed_outpoints.count(gs::outpoint(u->prev_tx_id, u->prev_out_idx)) == 0) {
                ret += u->value;
            }
        }
    }

    if (mempool_scriptpubkey_to_output.count(scriptpubkey) > 0) {
        const absl::flat_hash_set<gs::output*>& pk_utxos = mempool_scriptpubkey_to_output.at(scriptpubkey);

        for (const gs::output* u : pk_utxos) {
            ret += u->value;
        }
    }

    return ret;
}


}
