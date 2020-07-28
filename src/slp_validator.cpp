#include <vector>
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
        const auto p = transaction_map.insert({ tx.txid, tx });

        if (validate(tx.txid)) {
            return p.second;
        } else {
            return false;
        }
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

bool slp_validator::has(const gs::txid& txid) const
{
    return transaction_map.count(txid) == 1;
}

bool slp_validator::has_valid(const gs::txid& txid) const
{
    return valid.count(txid) == 1;
}

gs::transaction slp_validator::get(const gs::txid& txid) const
{
    return transaction_map.at(txid);
}

// #define ENABLE_SLP_VALIDATE_DEBUG_PRINTING

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
) {
#ifdef ENABLE_SLP_VALIDATE_DEBUG_PRINTING
    std::cerr << "send: " << tx.txid.decompress(true) << "\n";
#endif
    const auto & s = absl::get<gs::slp_transaction_send>(tx.slp.slp_tx);

    absl::uint128 output_amount = 0;
    for (const auto n : s.amounts) {
        output_amount += n;
    }

    absl::uint128 input_amount = 0;
    for (const auto & i_outpoint : tx.inputs) {
        VALIDATE_CONTINUE (! has_valid(i_outpoint.txid));

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
) {
    std::function<bool(
        const gs::transaction&,
        const gs::transaction&
    )> walk_mints_home = [&](
        const gs::transaction& front,
        const gs::transaction& back
    ) -> bool {
#ifdef ENABLE_SLP_VALIDATE_DEBUG_PRINTING
    std::cerr
        << "mint:"
        << " front " << front.txid.decompress(true)
        << " back "  << back.txid.decompress(true)
        << "\n";
#endif

        VALIDATE_CHECK (front.slp.tokenid    != back.slp.tokenid);
        VALIDATE_CHECK (front.slp.token_type != back.slp.token_type);

        for (const auto & i_outpoint : back.inputs) {
            VALIDATE_CONTINUE (! has_valid(i_outpoint.txid));

            const gs::transaction & txi = transaction_map.at(i_outpoint.txid);

            VALIDATE_CONTINUE (front.slp.tokenid    != txi.slp.tokenid);
            VALIDATE_CONTINUE (front.slp.token_type != txi.slp.token_type);
            VALIDATE_CONTINUE (i_outpoint != txi.mint_baton_outpoint());

            if (txi.slp.type == gs::slp_transaction_type::mint) {
                if (walk_mints_home(back, txi)) {
                    return true;
                }
            }
            else if (txi.slp.type == gs::slp_transaction_type::genesis) {
                return true;
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
) {
#ifdef ENABLE_SLP_VALIDATE_DEBUG_PRINTING
    std::cerr << "genesis: " << tx.txid.decompress(true) << "\n";
#endif
    if (tx.slp.token_type == 0x41) {
        VALIDATE_CHECK (tx.inputs.size() == 0);
        const gs::outpoint& i_outpoint = tx.inputs[0];
        VALIDATE_CHECK (! has_valid(i_outpoint.txid));

        const gs::transaction & txi = transaction_map.at(i_outpoint.txid);
        VALIDATE_CHECK (txi.slp.token_type != 0x81);
        VALIDATE_CHECK (txi.output_slp_amount(i_outpoint.vout) < 1);

        return validate(txi);
    }

    return true;
}

bool slp_validator::check_outputs_valid (
    absl::flat_hash_set<gs::txid> & seen,
    const gs::transaction & tx
) {
#ifdef ENABLE_SLP_VALIDATE_DEBUG_PRINTING
    std::cerr << "check_outputs_valid: " << tx.txid.decompress(true) << "\n";
#endif
    VALIDATE_CHECK (! has(tx.txid));

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

bool slp_validator::validate(const gs::transaction & tx)
{
#ifdef ENABLE_SLP_VALIDATE_DEBUG_PRINTING
    std::cerr << "validate(tx): " << tx.txid.decompress(true) << "\n";
#endif
    if (tx.slp.type == gs::slp_transaction_type::invalid) {
        return false;
    }

    if (has_valid(tx.txid)) {
        return true;
    }

    absl::flat_hash_set<gs::txid> seen;

    switch (tx.slp.type) {
        case gs::slp_transaction_type::send:    return check_send(seen, tx);
        case gs::slp_transaction_type::mint:    return check_mint(tx);
        case gs::slp_transaction_type::genesis: return check_genesis(tx);
        default: return false;
    }
}

bool slp_validator::validate(const gs::txid & txid)
{
#ifdef ENABLE_SLP_VALIDATE_DEBUG_PRINTING
    std::cerr << "validate(txid): " << txid.decompress(true) << "\n";
#endif
    VALIDATE_CHECK (! has(txid));
    const bool is_valid = validate(transaction_map.at(txid));
    if (is_valid) {
        add_valid_txid(txid);
    }

    return is_valid;
}

}
