#ifndef GS_SLPDB_HPP
#define GS_SLPDB_HPP

#include <absl/container/flat_hash_map.h>

#include <gs++/bhash.hpp>
#include <gs++/slp_token.hpp>
#include <gs++/slp_transaction.hpp>


namespace gs {

struct slpdb
{
    std::shared_mutex lookup_mtx; // IMPORTANT: lookups/inserts must be guarded with the lookup_mtx

    absl::flat_hash_map<gs::tokenid, gs::slp_token> tokens;
    // absl::flat_hash_map<gs::outpoint, gs::slp_output> utxos;

    void add_transaction(const gs::transaction& tx)
    {
        std::lock_guard lock(lookup_mtx);

        if (tx.slp.type == gs::slp_transaction_type::invalid) {
            // TODO remove utxos / cause burns here
            return;
        }

        if (tx.slp.type == gs::slp_transaction_type::genesis) {
            tokens.set({ tx.tokenid, gs::slp_token(tx) });
        }
    }
};

}

#endif

