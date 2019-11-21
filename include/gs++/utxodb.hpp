#ifndef GS_UTXODB_HPP
#define GS_UTXODB_HPP

#include <string>
#include <deque>
#include <boost/thread.hpp>
#include <absl/hash/internal/hash.h>
#include <absl/container/flat_hash_map.h>
#include <absl/container/node_hash_map.h>
#include <absl/container/flat_hash_set.h>
#include <gs++/output.hpp>
#include <gs++/rpc.hpp>
#include <gs++/scriptpubkey.hpp>

namespace gs {

struct utxodb
{
    constexpr static std::uint32_t rollback_depth { 10 };

    boost::shared_mutex lookup_mtx; // IMPORTANT: lookups/inserts must be guarded with the lookup_mtx

    std::uint32_t current_block_height;
    std::string   current_block_hash;

    absl::node_hash_map<gs::outpoint, gs::output>
    outpoint_map;
    
    absl::flat_hash_map<gs::scriptpubkey, absl::flat_hash_set<gs::output*>>
    scriptpubkey_to_output;

    absl::node_hash_map<gs::outpoint, gs::output>
    mempool_outpoint_map;

    absl::flat_hash_map<gs::scriptpubkey, absl::flat_hash_set<gs::output*>>
    mempool_scriptpubkey_to_output;

    absl::flat_hash_set<gs::outpoint>
    mempool_spent_confirmed_outpoints; // contains the outpoints that have been spent

    std::deque<std::vector<gs::output>>   last_block_removed;
    std::deque<std::vector<gs::outpoint>> last_block_added;

    utxodb();

    void rollback();

    std::vector<gs::output> get_outputs_by_outpoints(
        const std::vector<gs::outpoint> outpoints
    );

    std::vector<gs::output> get_outputs_by_scriptpubkey(
        const gs::scriptpubkey scriptpubkey,
        const std::uint32_t limit
    );

    std::uint64_t get_balance_by_scriptpubkey(
        const gs::scriptpubkey scriptpubkey
    );

    template <typename H>
    friend H AbslHashValue(H h, const utxodb& m)
    {
        return H::combine(
            std::move(h),
            m.outpoint_map,
            m.scriptpubkey_to_output,
            m.last_block_removed,
            m.last_block_added
        );
    }

};

}

#endif
