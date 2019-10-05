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
#include <3rdparty/picosha2.h>

#include <gs++/bhash.hpp>
#include <gs++/output.hpp>
#include <gs++/pk_script.hpp>
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

        gs::pk_script pk_script(script_len);
        std::copy(it, it+script_len, std::back_inserter(pk_script.v));
        it+=script_len;


        const gs::outpoint op(prev_tx_id, prev_out_idx);
        const gs::output out = gs::output(
            prev_tx_id,
            prev_out_idx,
            height,
            value,
            pk_script
        );

        if (! out.is_op_return()) {
            gs::output* const oid = &(*outpoint_map.insert({ op, out }).first).second;

            if (! pk_script_to_output.count(pk_script)) {
                pk_script_to_output.insert({ pk_script, { oid } });
            } else {
                pk_script_to_output[pk_script].emplace(oid);
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
        const std::uint32_t script_len = static_cast<std::uint32_t>(m.second.pk_script.size());
        outf.write(reinterpret_cast<const char *>(&script_len), sizeof(script_len));
        outf.write(reinterpret_cast<const char *>(m.second.pk_script.data()), m.second.pk_script.size());


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


// TODO we need to check for memory leaks/bugs
// they most likely would exist in this function 
// especially around the mempool utxo set
void utxodb::process_block(
    const std::vector<std::uint8_t>& block_data,
    const bool save_rollback
) {
    std::lock_guard lock(lookup_mtx);

    ++current_block_height;

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

    std::vector<gs::outpoint> blk_inputs;
    std::vector<gs::output>   blk_outputs;

    std::size_t total_added = 0;
    std::size_t total_removed = 0;

    for (std::uint64_t i=0; i<txn_count; ++i) {
        gs::transaction tx(it, current_block_height);

        for (auto & m : tx.inputs) {
            blk_inputs.emplace_back(m);
        }

        for (auto & m : tx.outputs) {
            if (m.is_op_return()) {
                continue;
            }

            blk_outputs.emplace_back(m);
            ++total_added;
        }
    }

    // only used if save_rollback enabled
    std::vector<gs::outpoint> this_block_added;
    std::vector<gs::output>   this_block_removed;

    for (auto & m : blk_outputs) {
        const gs::outpoint outpoint(m.prev_tx_id, m.prev_out_idx);
        gs::output* const oid = &(*outpoint_map.insert({ outpoint, m }).first).second;

        if (! pk_script_to_output.count(m.pk_script)) {
            pk_script_to_output.insert({ m.pk_script, { oid } });
        } else {
            pk_script_to_output[m.pk_script].emplace(oid);
        }

        mempool_outpoint_map.erase(outpoint);
        if (mempool_pk_script_to_output.count(m.pk_script) > 0) {
            mempool_pk_script_to_output.at(m.pk_script).erase(oid);
        }
        mempool_spent_confirmed_outpoints.erase(gs::outpoint(oid->prev_tx_id, oid->prev_out_idx));

        if (save_rollback) {
            this_block_added.push_back(outpoint);
        }
        // std::cout << height << "\tadded: " << m.prev_tx_id.decompress(true) << ":" << m.prev_out_idx << "\n";
    }

    for (auto & m : blk_inputs) {
        if (outpoint_map.count(m) > 0) {
            const gs::output& o = outpoint_map.at(m);

            if (pk_script_to_output.count(o.pk_script) > 0) {
                absl::flat_hash_set<gs::output*> & addr_map = pk_script_to_output.at(o.pk_script);

                if (addr_map.erase(&o)) {
                    // std::cout << height << "\tremoved: " << m.txid.decompress(true) << ":" << m.vout << "\n";
                }
                if (addr_map.empty()) {
                    pk_script_to_output.erase(o.pk_script);
                }

                mempool_spent_confirmed_outpoints.erase(gs::outpoint(o.prev_tx_id, o.prev_out_idx));
            }

            if (save_rollback) {
                this_block_removed.push_back(o);
            }

            if (outpoint_map.erase(m)) {
                ++total_removed;
            }
        }

        if (mempool_outpoint_map.count(m) > 0) {
            const gs::output& o = mempool_outpoint_map.at(m);

            if (mempool_pk_script_to_output.count(o.pk_script) > 0) {
                absl::flat_hash_set<gs::output*> & addr_map = mempool_pk_script_to_output.at(o.pk_script);

                if (addr_map.erase(&o)) {
                    // std::cout << height << "\tremoved: " << m.txid.decompress(true) << ":" << m.vout << "\n";
                }
                if (addr_map.empty()) {
                    mempool_pk_script_to_output.erase(o.pk_script);
                }
            }

            if (save_rollback) {
                this_block_removed.push_back(o);
            }

            if (mempool_outpoint_map.erase(m)) {
                ++total_removed;
            }
        }
    }

    if (save_rollback) {
        last_block_added.push_back(this_block_added);
        last_block_removed.push_back(this_block_removed);

        if (last_block_added.size() > rollback_depth) {
            last_block_added.pop_front();
        }
        if (last_block_removed.size() > rollback_depth) {
            last_block_removed.pop_front();
        }
    }

    spdlog::info("processed block {} +{} -{}", current_block_height, total_added, total_removed);
}

void utxodb::process_mempool_tx(const std::vector<std::uint8_t>& msg_data)
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
        gs::output* const oid = &(*mempool_outpoint_map.insert({ outpoint, m }).first).second;

        if (! pk_script_to_output.count(m.pk_script)) {
            mempool_pk_script_to_output.insert({ m.pk_script, { oid } });
        } else {
            mempool_pk_script_to_output[m.pk_script].emplace(oid);
        }
    }

    for (auto & m : tx.inputs) {
        // std::cout << "\tinput txid: " << m.txid.decompress(true) << ":" << m.vout << "\n";

        if (mempool_outpoint_map.count(m) == 0) {
            mempool_spent_confirmed_outpoints.insert(m);
            continue;
        }

        const gs::output& o = mempool_outpoint_map.at(m);

        if (! mempool_pk_script_to_output.count(o.pk_script)) {
            continue;
        }
        absl::flat_hash_set<gs::output*> & addr_map = mempool_pk_script_to_output.at(o.pk_script);
        if (addr_map.erase(&o)) {
            // std::cout << height << "\tremoved: " << m.txid.decompress(true) << ":" << m.vout << "\n";
        }
        if (addr_map.empty()) {
            mempool_pk_script_to_output.erase(o.pk_script);
        }
    }
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

        if (! pk_script_to_output.count(m.pk_script)) {
            pk_script_to_output.insert({ m.pk_script, { oid } });
        } else {
            pk_script_to_output[m.pk_script].emplace(oid);
        }

        // std::cout << "\tadded: " << m.prev_tx_id.decompress(true) << ":" << m.prev_out_idx << "\n";
    }

    for (auto m : rb_added) {
        if (outpoint_map.count(m) == 0) {
            continue;
        }

        const gs::output& o           = outpoint_map.at(m);
        const gs::pk_script pk_script = o.pk_script;

        if (! pk_script_to_output.count(pk_script)) {
            continue;
        }
        absl::flat_hash_set<gs::output*> & addr_map = pk_script_to_output.at(pk_script);
        if (addr_map.erase(&o)) {
            // std::cout << "\tremoved: " << m.txid.decompress(true) << ":" << m.vout << "\n";
            if (addr_map.empty()) {
                pk_script_to_output.erase(pk_script);
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

std::vector<gs::output> utxodb::get_outputs_by_pubkey(
    const gs::pk_script pk_script
) {
    std::shared_lock lock(lookup_mtx);

    std::vector<gs::output> ret;

    if (pk_script_to_output.count(pk_script) > 0) {
        const absl::flat_hash_set<gs::output*>& pk_utxos = pk_script_to_output.at(pk_script);

        for (const gs::output* u : pk_utxos) {
            if (mempool_spent_confirmed_outpoints.count(gs::outpoint(u->prev_tx_id, u->prev_out_idx)) == 0) {
                ret.push_back(*u);
            }
        }
    }

    if (mempool_pk_script_to_output.count(pk_script) > 0) {
        const absl::flat_hash_set<gs::output*>& pk_utxos = mempool_pk_script_to_output.at(pk_script);

        for (const gs::output* u : pk_utxos) {
            ret.push_back(*u);
        }
    }

    return ret;
}


}
