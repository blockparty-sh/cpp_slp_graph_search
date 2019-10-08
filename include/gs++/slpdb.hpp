#ifndef GS_SLPDB_HPP
#define GS_SLPDB_HPP

#include <variant>
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
            gs::slp_token* const oid = &(*tokens.insert({ gs::tokenid(tx.txid.v), gs::slp_token(tx) }).first).second;
            oid->add_transaction(tx);
        }
        else if (tx.slp.type == gs::slp_transaction_type::mint) {
            const auto slp = std::get<gs::slp_transaction_mint>(tx.slp.slp_tx);
            auto token_search = tokens.find(slp.tokenid);
            if (token_search == tokens.end()) {
                return;
            }

            token_search->second.add_transaction(tx);
        }
        else if (tx.slp.type == gs::slp_transaction_type::send) {
            const auto slp = std::get<gs::slp_transaction_send>(tx.slp.slp_tx);
            auto token_search = tokens.find(slp.tokenid);
            if (token_search == tokens.end()) {
                return;
            }

            token_search->second.add_transaction(tx);
        }
    }
};

}

#endif

