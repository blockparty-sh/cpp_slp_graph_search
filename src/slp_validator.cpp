#include <vector>
#include <cassert>
#include <cstdint>
#include <functional>

#include <absl/types/variant.h>
#include <absl/container/flat_hash_map.h>
#include <absl/container/flat_hash_set.h>
#include <absl/numeric/int128.h>

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

#define ENABLE_SLP_VALIDATE_DEBUG_PRINTING

#ifdef ENABLE_SLP_VALIDATE_DEBUG_PRINTING
    #define VALIDATE_CHECK(cond) {\
        if (cond) { \
            std::cerr << "slp_validate:check\t" << #cond << "\tline: " << __LINE__ << "\n";\
            return false;\
        }\
    }
    #define VALIDATE_CONTINUE(cond) {\
        if (cond) { \
            std::cerr << "slp_validate:skip\t" << #cond << "\tline: " << __LINE__ << "\n";\
            continue;\
        }\
    }
#else
    #define VALIDATE_CHECK(cond) {\
        if (cond) { \
            return false;\
        }\
    }
    #define VALIDATE_CONTINUE(cond) {\
        if (cond) { \
            continue;\
        }\
    }
#endif


bool slp_validator::check_send(
    absl::flat_hash_set<gs::txid> & seen,
    const gs::transaction & tx
) const {
    const auto & s = absl::get<gs::slp_transaction_send>(tx.slp.slp_tx);

    absl::uint128 output_amount = 0;
    for (auto n : s.amounts) {
        output_amount += n;
    }

    absl::uint128 input_amount = 0;
    for (auto & i_outpoint : tx.inputs) {
        VALIDATE_CONTINUE (transaction_map.count(i_outpoint.txid) == 0);

        const gs::transaction & txi = transaction_map.at(i_outpoint.txid);

        VALIDATE_CONTINUE (tx.slp.token_type != txi.slp.token_type);
        VALIDATE_CONTINUE (tx.slp.tokenid    != txi.slp.tokenid);
        VALIDATE_CONTINUE (! check_outputs_valid(seen, txi));

        input_amount += txi.output_slp_amount(i_outpoint.vout);
    }

    VALIDATE_CHECK (output_amount > input_amount);

    return true;
}

bool slp_validator::check_mint(
    const gs::transaction & tx
) const {
    std::function<bool(
        const gs::transaction&,
        const gs::transaction&
    )> walk_mints_home = [&](
        const gs::transaction& front,
        const gs::transaction& back
    ) -> bool {
        assert(! mints.empty());

        VALIDATE_CHECK (front.slp.tokenid    != back.slp.tokenid);
        VALIDATE_CHECK (front.slp.token_type != back.slp.token_type);

        for (auto & i_outpoint : back.inputs) {
            VALIDATE_CONTINUE (transaction_map.count(i_outpoint.txid) == 0);

            const gs::transaction & txi = transaction_map.at(i_outpoint.txid);

            VALIDATE_CONTINUE (front.slp.tokenid    != txi.slp.tokenid);
            VALIDATE_CONTINUE (front.slp.token_type != txi.slp.token_type);

            if (txi.slp.type == gs::slp_transaction_type::mint) {
                const gs::outpoint mint_baton_outpoint = txi.mint_baton_outpoint();
                if (i_outpoint == mint_baton_outpoint) {
                    return walk_mints_home(back, txi); 
                }
            }
            else if (txi.slp.type == gs::slp_transaction_type::genesis) {
                const gs::outpoint mint_baton_outpoint = txi.mint_baton_outpoint();
                if (i_outpoint == mint_baton_outpoint) {
                    return true;
                }
            }
        }

        if (valid.count(back.txid) > 0) {
            return true;
        }

        return false;
    };

    VALIDATE_CHECK (! walk_mints_home(tx, tx));

    return true;
}

bool slp_validator::check_genesis(
    const gs::transaction & tx
) const {
    std::function<bool(
        const gs::transaction& tx
    )> nft1_child_genesis_validity_check = [&](
        const gs::transaction& tx
    ) -> bool {
        for (auto & i_outpoint : tx.inputs) {
            VALIDATE_CONTINUE (transaction_map.count(i_outpoint.txid) == 0);

            const gs::transaction & txi = transaction_map.at(i_outpoint.txid);
            
            if (txi.slp.token_type == 0x81) {
                VALIDATE_CONTINUE (txi.output_slp_amount(i_outpoint.vout) < 1);

                absl::flat_hash_set<gs::txid> seen;
                if (check_outputs_valid(seen, txi)) {
                    return true;
                }
            }
        }

        return false;
    };

    if (tx.slp.token_type == 0x41) {
        VALIDATE_CHECK (! nft1_child_genesis_validity_check(tx));
    }

    return true;
}

bool slp_validator::check_outputs_valid (
    absl::flat_hash_set<gs::txid> & seen,
    const gs::transaction & tx
) const {
    VALIDATE_CHECK (transaction_map.count(tx.txid) == 0);

    // already has been validated during search
    if (! seen.insert(tx.txid).second) {
        return true;
    }

    if (valid.count(tx.txid) > 0) {
        return true;
    }

    switch (tx.slp.type) {
        case gs::slp_transaction_type::send:    return check_send(seen, tx);
        case gs::slp_transaction_type::mint:    return check_mint(tx);
        case gs::slp_transaction_type::genesis: return check_genesis(tx);
        default: return false;
    }
}

bool slp_validator::validate(const gs::transaction & tx) const
{
    absl::flat_hash_set<gs::txid> seen;

    switch (tx.slp.type) {
        case gs::slp_transaction_type::send:    return check_send(seen, tx);
        case gs::slp_transaction_type::mint:    return check_mint(tx);
        case gs::slp_transaction_type::genesis: return check_genesis(tx);
        default: return false;
    }
}

bool slp_validator::validate(const gs::txid & txid) const
{
    VALIDATE_CHECK (transaction_map.count(txid) == 0);
    return validate(transaction_map.at(txid));
}

}
