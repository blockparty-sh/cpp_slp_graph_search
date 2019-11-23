#ifndef GS_SLP_VALIDATOR_HPP
#define GS_SLP_VALIDATOR_HPP

#include <vector>

#include <absl/container/flat_hash_map.h>
#include <absl/container/flat_hash_set.h>
#include <absl/numeric/int128.h>

#include <gs++/transaction.hpp>
#include <gs++/bhash.hpp>


namespace gs {

struct slp_validator
{
    absl::flat_hash_map<gs::txid, gs::transaction> transaction_map;
    absl::flat_hash_set<gs::txid> valid;

    slp_validator() = default;

    bool add_tx(const gs::transaction& tx);
    bool remove_tx(const gs::txid& txid);
    bool add_valid_txid(const gs::txid& txid);
    bool has(const gs::txid& txid) const;

    bool check_outputs_valid(
        absl::flat_hash_set<gs::txid> & seen,
        const gs::transaction & tx
    ) const;

    bool check_send(
        absl::flat_hash_set<gs::txid> & seen,
        const gs::transaction & tx
    ) const;
    bool check_mint(const gs::transaction & tx) const;
    bool check_genesis(const gs::transaction & tx) const;

    bool validate(const gs::transaction & tx) const;
    bool validate(const gs::txid & txid) const;
};

}

#endif
