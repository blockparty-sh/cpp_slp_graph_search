#include <vector>

#include <absl/types/variant.h>
#include <absl/container/flat_hash_map.h>
#include <absl/container/flat_hash_set.h>
#include <absl/numeric/int128.h>

#include <cassert>
#include <cstdint>

#include <gs++/slp_validator.hpp>
#include <gs++/transaction.hpp>
#include <gs++/bhash.hpp>
#include <gs++/slp_transaction.hpp>

namespace gs {

bool slp_validator::add_tx(const gs::transaction& tx)
{
    if (tx.slp.type != gs::slp_transaction_type::invalid) {
        transaction_map.insert({ tx.txid, tx });
        return true;
    }

    return false;
}

bool slp_validator::remove_tx(const gs::txid& txid)
{
    return transaction_map.erase(txid) > 0;
}

bool slp_validator::add_valid_txid(const gs::txid& txid)
{
    return valid.insert(txid).second;
}

#define ENABLE_SLP_VALIDATE_ERROR_PRINTING

#ifdef ENABLE_SLP_VALIDATE_ERROR_PRINTING
    #define VALIDATE_CHECK(cond) {\
        if (cond) { \
            std::cerr << #cond << "\tline: " << __LINE__ << "\n";\
            return false;\
        }\
    }
#else
    #define VALIDATE_CHECK(cond) {\
        if (cond) { \
            return false;\
        }\
    }
#endif

bool slp_validator::walk_mints_home (
    std::vector<gs::transaction>& mints
) const {
    assert(! mints.empty());

    auto & tx = mints.back();
    VALIDATE_CHECK (mints.front().slp.tokenid != tx.slp.tokenid);
    VALIDATE_CHECK (mints.front().slp.token_type != tx.slp.token_type);

    for (auto & i_outpoint : tx.inputs) {
        if (transaction_map.count(i_outpoint.txid) == 0) {
            continue;
        }

        const gs::transaction & txi = transaction_map.at(i_outpoint.txid);

        if (mints.front().slp.tokenid != txi.slp.tokenid) {
            continue;
        }
        if (mints.front().slp.token_type != txi.slp.token_type) {
            continue;
        }

        if (txi.slp.type == gs::slp_transaction_type::mint) {
            const gs::outpoint mint_baton_outpoint = txi.mint_baton_outpoint();
            if (mint_baton_outpoint.vout > 0 && i_outpoint == mint_baton_outpoint) {
                mints.push_back(txi);
                return walk_mints_home(mints); 
            }
        }
        else if (txi.slp.type == gs::slp_transaction_type::genesis) {
            const gs::outpoint mint_baton_outpoint = txi.mint_baton_outpoint();
            if (mint_baton_outpoint.vout > 0 && i_outpoint == mint_baton_outpoint) {
                return true;
            }
        }
    }

    if (valid.count(tx.txid) > 0) {
        // TODO What about different token_type
        return true;
    }

    return false;
}

// This is used for NFT1-Child Genesis validity check
// When the input is an SLP-invalid BCH-only tx, the GENESIS tx should be SLP-
// invalid since it is not token type 1 or 129 (NFT1 parent)
//
// TODO what happens if there are 2 different inputs here?
// what if there are nft parent inputs here
// what if there are multiple genesis txs 
// etc
bool slp_validator::nft1_child_genesis_validity_check(
    const gs::transaction& tx
) const {
    for (auto & i_outpoint : tx.inputs) {
        if (transaction_map.count(i_outpoint.txid) == 0) {
            continue;
        }

        const gs::transaction & txi = transaction_map.at(i_outpoint.txid);
        
        if (txi.slp.token_type == 0x81) {
            if (txi.output_slp_amount(i_outpoint.vout) < 1) {
                continue;
            }

            absl::flat_hash_set<gs::txid> seen;
            if (check_outputs_valid(seen, txi)) {
                return true;
            }
        }
    }

    return false;
}

bool slp_validator::check_send(
    absl::flat_hash_set<gs::txid> & seen,
    const gs::transaction & tx
) const {
    const auto & s = absl::get<gs::slp_transaction_send>(tx.slp.slp_tx);

    absl::uint128 output_amount = 0;
    for (const std::uint64_t n : s.amounts) {
        output_amount += n;
    }

    absl::uint128 input_amount = 0;
    for (auto & i_outpoint : tx.inputs) {
        if (transaction_map.count(i_outpoint.txid) == 0) {
            continue;
        }

        const gs::transaction & txi               = transaction_map.at(i_outpoint.txid);
        const std::uint64_t     slp_output_amount = txi.output_slp_amount(i_outpoint.vout);

        if (tx.slp.token_type != txi.slp.token_type) {
            continue;
        }

        if (tx.slp.tokenid != txi.slp.tokenid) {
            continue;
        }

        if (! check_outputs_valid(seen, txi)) {
            continue;
        }

        input_amount += slp_output_amount;
    }

    VALIDATE_CHECK (output_amount > input_amount);

    return true;
}

bool slp_validator::check_mint(
    const gs::transaction & tx
) const {
    std::vector<gs::transaction> mints;
    mints.push_back(tx);
    VALIDATE_CHECK (! walk_mints_home(mints));

    return true;
}

bool slp_validator::check_genesis(
    const gs::transaction & tx
) const {
    const auto & s = absl::get<gs::slp_transaction_genesis>(tx.slp.slp_tx);

    if (tx.slp.token_type == 0x41) {
        VALIDATE_CHECK (! nft1_child_genesis_validity_check(tx));
    }

    return true;
}

bool slp_validator::check_outputs_valid (
    absl::flat_hash_set<gs::txid> & seen,
    const gs::transaction & tx
) const {
    if (transaction_map.count(tx.txid) == 0) {
        return false;
    }

    if (! seen.insert(tx.txid).second) {
        return true;
    }

    if (valid.count(tx.txid) > 0) {
        return true;
    }

    if (tx.slp.type == gs::slp_transaction_type::send) {
        return check_send(seen, tx);
    }
    else if (tx.slp.type == gs::slp_transaction_type::mint) {
        return check_mint(tx);
    }
    else if (tx.slp.type == gs::slp_transaction_type::genesis) {
        return check_genesis(tx);
    }

    return false;
}

// TODO should this take gs::transaction rather than txid?
// or maybe allow for both

bool slp_validator::validate(const gs::transaction & tx) const
{
    absl::flat_hash_set<gs::txid> seen;

    if (tx.slp.type == gs::slp_transaction_type::send) {
        return check_send(seen, tx);
    }
    else if (tx.slp.type == gs::slp_transaction_type::mint) {
        return check_mint(tx);
    }
    else if (tx.slp.type == gs::slp_transaction_type::genesis) {
        return check_genesis(tx);
    }

    return false;
}

bool slp_validator::validate(const gs::txid & txid) const
{
    VALIDATE_CHECK (transaction_map.count(txid) == 0);
    return validate(transaction_map.at(txid));
}

}
