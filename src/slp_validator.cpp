#include <vector>

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

void slp_validator::add_tx(const gs::transaction& tx)
{
    transaction_map.insert({ tx.txid, tx });
}

bool slp_validator::walk_mints_home (
    absl::flat_hash_set<gs::txid> & seen,
    std::vector<gs::transaction>& mints
) const {
    assert(! mints.empty());

    auto tx = mints.back();
    seen.insert(tx.txid);

    for (auto & i_outpoint : tx.inputs) {
        if (transaction_map.count(i_outpoint.txid) > 0) {
            const gs::transaction & txi = transaction_map.at(i_outpoint.txid);

            if (transaction_map.count(i_outpoint.txid) == 0) {
                return false;
            }

            if (txi.slp.type == gs::slp_transaction_type::mint) {
                const auto & s = std::get<gs::slp_transaction_mint>(txi.slp.slp_tx);

                if (s.has_mint_baton) {
                    mints.push_back(txi);
                    return walk_mints_home(seen, mints); 
                }
            }
            else if (txi.slp.type == gs::slp_transaction_type::genesis) {
                const auto & s = std::get<gs::slp_transaction_genesis>(txi.slp.slp_tx);

                if (s.has_mint_baton) {
                    mints.push_back(txi);
                    return true;
                }
            }
        }
    }

    return false;
}

bool slp_validator::check_outputs_valid (
    absl::flat_hash_set<gs::txid> & seen,
    const gs::txid & txid
) const {
    // std::cout << txid.decompress(true) << "\n";

    // already was in set
    if (! seen.insert(txid).second) {
        return true;
    }

    if (transaction_map.count(txid) == 0) {
        return true;
    }


    auto & tx = transaction_map.at(txid);

    if (tx.slp.type == gs::slp_transaction_type::send) {
        const auto & s = std::get<gs::slp_transaction_send>(tx.slp.slp_tx);

        absl::uint128 output_amount = 0;
        for (const std::uint64_t n : s.amounts) {
            output_amount += n;
        }

        absl::uint128 input_amount = 0;
        for (auto & i_outpoint : tx.inputs) {
            if (transaction_map.count(i_outpoint.txid) > 0) {
                const gs::transaction & txi               = transaction_map.at(i_outpoint.txid);
                const std::uint64_t     slp_output_amount = txi.output_slp_amount(i_outpoint.vout);
                input_amount += slp_output_amount;

                if (! check_outputs_valid(seen, i_outpoint.txid)) {
                    // std::cout << "!check_outputs_valid: "  << i_outpoint.txid.decompress(true) << "\n";
                    return false;
                }
            }
        }

        if (output_amount > input_amount) {
            // std::cout << "output_amount > input_amount : "  << txid.decompress(true) << "\n";
            return false;
        }
    }
    else if (tx.slp.type == gs::slp_transaction_type::mint) {
        std::vector<gs::transaction> mints;
        mints.push_back(tx);
        // TODO ensure mints have baton moving correctly
        if (! walk_mints_home(seen, mints)) {
            return false;
        }
    }
    /*
    else if (tx.slp.type == gs::slp_transaction_type::genesis) {
        const auto & s = std::get<gs::slp_transaction_genesis>(tx.slp.slp_tx);
        output_amount = s.qty;
        input_amount  = s.qty;
    }
    */


    return true;
}

bool slp_validator::validate(const gs::txid & txid) const
{
    absl::flat_hash_set<gs::txid> seen;
    return check_outputs_valid(seen, txid);
}

}
