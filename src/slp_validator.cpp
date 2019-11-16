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

bool slp_validator::walk_mints_home (
    absl::flat_hash_set<gs::txid> & seen,
    std::vector<gs::transaction>& mints
) const {
    assert(! mints.empty());

    auto & tx = mints.back();
    if (mints.front().slp.token_type != tx.slp.token_type) {
        std::cout << "walk_mints_home token_type not original token_type\n";
        return false;
    }
    seen.insert(tx.txid);

    for (auto & i_outpoint : tx.inputs) {
        if (transaction_map.count(i_outpoint.txid) > 0) {
            if (transaction_map.count(i_outpoint.txid) == 0) {
                // TODO should this be continue ?
                continue;
            }

            const gs::transaction & txi = transaction_map.at(i_outpoint.txid);

            if (txi.slp.type == gs::slp_transaction_type::mint) {
                const auto & s = absl::get<gs::slp_transaction_mint>(txi.slp.slp_tx);

                if (s.has_mint_baton) {
                    mints.push_back(txi);
                    return walk_mints_home(seen, mints); 
                }
            }
            else if (txi.slp.type == gs::slp_transaction_type::genesis) {
                const auto & s = absl::get<gs::slp_transaction_genesis>(txi.slp.slp_tx);

                if (s.has_mint_baton) {
                    mints.push_back(txi);
                    return true;
                }
            }
        }
    }

    if (valid.count(tx.txid)) {
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
bool slp_validator::has_input_which_is_slp_valid(
    const gs::transaction& tx
) const {
    for (auto & i_outpoint : tx.inputs) {
        if (transaction_map.count(i_outpoint.txid) == 0) {
            continue;
        }

        const gs::transaction & txi = transaction_map.at(i_outpoint.txid);

        if (txi.slp.type != gs::slp_transaction_type::invalid) {
            return true;
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
        const auto & s = absl::get<gs::slp_transaction_send>(tx.slp.slp_tx);

        absl::uint128 output_amount = 0;
        for (const std::uint64_t n : s.amounts) {
            output_amount += n;
        }

        absl::uint128 input_amount = 0;
        for (auto & i_outpoint : tx.inputs) {
            if (transaction_map.count(i_outpoint.txid) > 0) {
                if (! check_outputs_valid(seen, i_outpoint.txid)) {
                    std::cout << "!check_outputs_valid: "  << i_outpoint.txid.decompress(true) << "\n";
                    // return false;
                    continue;
                }
                const gs::transaction & txi               = transaction_map.at(i_outpoint.txid);
                const std::uint64_t     slp_output_amount = txi.output_slp_amount(i_outpoint.vout);
                input_amount += slp_output_amount;
            }
        }

        if (output_amount > input_amount) {
            std::cout << "output_amount > input_amount : "  << txid.decompress(true) << "\n";
            return false;
        }
    }
    else if (tx.slp.type == gs::slp_transaction_type::mint) {
        std::vector<gs::transaction> mints;
        mints.push_back(tx);
        // TODO ensure mints have baton moving correctly
        if (! walk_mints_home(seen, mints)) {
            std::cout << "! walk_mints_home\n";
            return false;
        }
    }
    else if (tx.slp.type == gs::slp_transaction_type::genesis) {
        std::cout << "genesus" << std::endl;
        std::cout << "token_type : " << tx.slp.token_type << std::endl;
        const auto & s = absl::get<gs::slp_transaction_genesis>(tx.slp.slp_tx);

        if (tx.slp.token_type == 0x41) {
            std::cout << "child token_type" << std::endl;
            // TODO what happens if there are 2 different inputs here?
            if (! has_input_which_is_slp_valid(tx)) {
                std::cout << "not has_input_which_is_slp_valid" << std::endl;
                return false;
            }
        }
    }


    return true;
}

// TODO should this take gs::transaction rather than txid?
// or maybe allow for both
bool slp_validator::validate(const gs::txid & txid) const
{
    auto & tx = transaction_map.at(txid);
    if (tx.slp.type == gs::slp_transaction_type::invalid) {
        return false;
    }

    absl::flat_hash_set<gs::txid> seen;
    return check_outputs_valid(seen, txid);
}

bool slp_validator::validate(const gs::transaction & tx) const
{
    absl::flat_hash_set<gs::txid> seen;

    if (tx.slp.type == gs::slp_transaction_type::send) {
        const auto & s = absl::get<gs::slp_transaction_send>(tx.slp.slp_tx);

        absl::uint128 output_amount = 0;
        for (const std::uint64_t n : s.amounts) {
            output_amount += n;
        }

        absl::uint128 input_amount = 0;
        for (auto & i_outpoint : tx.inputs) {
            if (transaction_map.count(i_outpoint.txid) > 0) {
                if (! check_outputs_valid(seen, i_outpoint.txid)) {
                    std::cout << "!check_outputs_valid: "  << i_outpoint.txid.decompress(true) << "\n";
                    // return false;
                    continue;
                }
                const gs::transaction & txi               = transaction_map.at(i_outpoint.txid);
                const std::uint64_t     slp_output_amount = txi.output_slp_amount(i_outpoint.vout);
                input_amount += slp_output_amount;
            }
        }

        if (output_amount > input_amount) {
            std::cout << "output_amount > input_amount : "  << tx.txid.decompress(true) << "\n";
            return false;
        }
    }
    else if (tx.slp.type == gs::slp_transaction_type::mint) {
        std::vector<gs::transaction> mints;
        mints.push_back(tx);

        // TODO ensure mints have baton moving correctly
        if (! walk_mints_home(seen, mints)) {
            std::cout << "! walk_mints_home\n";
            return false;
        }
    }
    else if (tx.slp.type == gs::slp_transaction_type::genesis) {
        std::cout << "genesus" << std::endl;
        std::cout << "token_type : " << tx.slp.token_type << std::endl;
        const auto & s = absl::get<gs::slp_transaction_genesis>(tx.slp.slp_tx);

        if (tx.slp.token_type == 0x41) {
            std::cout << "child token_type" << std::endl;
            // TODO what happens if there are 2 different inputs here?
            if (! has_input_which_is_slp_valid(tx)) {
                std::cout << "not has_input_which_is_slp_valid" << std::endl;
                return false;
            }
        }
    }
    else if (tx.slp.type == gs::slp_transaction_type::invalid) {
        return false;
    }

    return true;
}

}
