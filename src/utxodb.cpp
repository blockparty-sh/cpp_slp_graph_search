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

#include <absl/container/flat_hash_map.h>
#include <absl/container/node_hash_map.h>
#include <absl/container/flat_hash_set.h>

#include <mio/mmap.hpp>
#include <zmq.hpp>
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

bool utxodb::load_from_bchd_checkpoint (
    const std::string & path,
    const std::uint32_t block_height,
    const std::string & block_hash
) {
    std::lock_guard lock(lookup_mtx);

    const int fd = open(path.c_str(), O_RDONLY);
    if (fd == -1) {
        return false;
    }

    outpoint_map.reserve(40000000);
    mio::mmap_source mmap(fd, 0, mio::map_entire_file);

    std::size_t i =0;
    for (auto it = mmap.begin(); it != mmap.end();) {
        gs::txid prev_tx_id;
        std::copy(it, it+32, reinterpret_cast<char*>(prev_tx_id.begin()));
        it+=32;

        std::uint32_t prev_out_idx;
        std::copy(it, it+4, reinterpret_cast<char*>(&prev_out_idx));
        it+=4;


        std::uint32_t height;
        std::copy(it, it+4, reinterpret_cast<char*>(&height));
        it+=4;

        std::uint64_t value;
        std::copy(it, it+8, reinterpret_cast<char*>(&value));
        it+=8;

        bool is_coinbase; // TODO

        std::uint32_t script_len;
        std::copy(it, it+4, reinterpret_cast<std::uint8_t*>(&script_len));
        it+=4;

        gs::scriptpubkey scriptpubkey(script_len);
        std::copy(it, it+script_len, std::back_inserter(scriptpubkey.v));
        it+=script_len;


        const gs::outpoint op(prev_tx_id, prev_out_idx);
        const gs::output out = gs::output(
            prev_tx_id,
            prev_out_idx,
            height,
            value,
            scriptpubkey
        );

        if (! out.is_op_return()) {
            gs::output* const oid = &(*outpoint_map.insert({ op, out }).first).second;

            if (! scriptpubkey_to_output.count(scriptpubkey)) {
                scriptpubkey_to_output.insert({ scriptpubkey, { oid } });
            } else {
                scriptpubkey_to_output[scriptpubkey].emplace(oid);
            }
        }

        // std::cout << prev_tx_id.decompress(true) << "\t" << prev_out_idx << std::endl; 
        /*
        std::cout << (int) is_coinbase << std::endl;
        std::cout << height << std::endl;
        std::cout << value << std::endl;
        std::cout << script_len << std::endl;
        */
        ++i;
        if (i % 10000 == 0) {
            std::cout << i << "\t" << prev_tx_id.decompress(true) << "\n";
        }

        // // TODO TESTING
        // if (i % 1000000 == 0) {
        //     break;
        // }
    }

    current_block_height = block_height;
    current_block_hash   = block_hash;

    return true;
}

bool utxodb::save_bchd_checkpoint (
    const std::string & path
) {
    std::lock_guard lock(lookup_mtx);

    std::ofstream outf(path, std::ofstream::binary);

    std::size_t i =0;

    /*
    std::vector<gs::output> outputs;
    outputs.reserve(outpoint_map.size());

    for (std::pair<gs::outpoint, gs::output> m : outpoint_map) {
        outputs.emplace_back(m.second);
    }

    std::sort(outputs.begin(), outputs.end(), [](gs::output a, gs::output b) -> bool {
        for (std::size_t i=0; i<a.prev_tx_id.v.size(); ++i) {
            if (a.prev_tx_id.v[i] < b.prev_tx_id.v[i]) return 1;
            if (a.prev_tx_id.v[i] > b.prev_tx_id.v[i]) return 0;
        }

        return a.prev_out_idx < b.prev_out_idx;
    });
    */

    for (std::pair<gs::outpoint, gs::output> m : outpoint_map) {
        // std::cout << m.second.prev_tx_id.decompress(true) << ":" << m.second.prev_out_idx << "\n";
        outf.write(reinterpret_cast<const char *>(m.second.prev_tx_id.data()), m.second.prev_tx_id.size());
        outf.write(reinterpret_cast<const char *>(&m.second.prev_out_idx), sizeof(m.second.prev_out_idx));
        outf.write(reinterpret_cast<const char *>(&m.second.height), sizeof(m.second.height));
        outf.write(reinterpret_cast<const char *>(&m.second.value), sizeof(m.second.value));
        const std::uint32_t script_len = static_cast<std::uint32_t>(m.second.scriptpubkey.size());
        outf.write(reinterpret_cast<const char *>(&script_len), sizeof(script_len));
        outf.write(reinterpret_cast<const char *>(m.second.scriptpubkey.data()), m.second.scriptpubkey.size());


        // std::cout << prev_tx_id.decompress(true) << "\t" << prev_out_idx << std::endl; 
        /*
        std::cout << (int) is_coinbase << std::endl;
        std::cout << height << std::endl;
        std::cout << value << std::endl;
        std::cout << script_len << std::endl;
        */
        ++i;
    }

    spdlog::info("saved utxo checkpoint: {}", path);

    return true;
}


void utxodb::rollback()
{
    std::lock_guard lock(lookup_mtx);

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
            scriptpubkey_to_output[m.scriptpubkey].emplace(oid);
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
    std::shared_lock lock(lookup_mtx);

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
    std::shared_lock lock(lookup_mtx);

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
    std::shared_lock lock(lookup_mtx);

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
