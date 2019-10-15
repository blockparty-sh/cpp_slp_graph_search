#ifndef GS_SLP_TOKEN_HPP
#define GS_SLP_TOKEN_HPP

#include <cassert>
#include <variant>
#include <optional>

#include <absl/container/flat_hash_map.h>
#include <absl/container/node_hash_map.h>

#include <gs++/bhash.hpp>
#include <gs++/output.hpp>
#include <gs++/transaction.hpp>
#include <gs++/slp_transaction.hpp>


namespace gs {

struct slp_token
{
    gs::tokenid tokenid;

    absl::flat_hash_map<gs::txid, gs::transaction> transactions;
    absl::flat_hash_map<gs::outpoint, gs::slp_output> utxos;
    std::optional<gs::outpoint> mint_baton_outpoint;
    
    slp_token()
    {}

    slp_token(const gs::transaction& tx)
    : tokenid(gs::tokenid(tx.txid.v))
    , transactions({{ tx.txid, tx }})
    {
        assert(tx.slp.type == gs::slp_transaction_type::genesis);
    }
};

}

#endif
