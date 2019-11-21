#ifndef GS_SLPDB_HPP
#define GS_SLPDB_HPP

#include <utility>

#include <boost/thread.hpp>
#include <absl/types/variant.h>
#include <absl/container/flat_hash_map.h>
#include <absl/numeric/int128.h>
#include <spdlog/spdlog.h>

#include <gs++/bhash.hpp>
#include <gs++/slp_token.hpp>
#include <gs++/slp_transaction.hpp>


namespace gs {

struct slpdb
{
    boost::shared_mutex lookup_mtx; // IMPORTANT: lookups/inserts must be guarded with the lookup_mtx

    absl::flat_hash_map<gs::tokenid, gs::slp_token> tokens;
    absl::flat_hash_map<gs::outpoint, gs::tokenid> utxo_to_tokenid;

    void add_transaction(const gs::transaction& tx)
    {
        boost::lock_guard<boost::shared_mutex> lock(lookup_mtx);

        if (tx.slp.type == gs::slp_transaction_type::invalid) {
            // TODO remove utxos / cause burns here
            return;
        }
        // std::cout << tx.txid.decompress(true) << "\n";

        if (tx.slp.type == gs::slp_transaction_type::genesis) {
            const auto slp = absl::get<gs::slp_transaction_genesis>(tx.slp.slp_tx);

            // spdlog::info("genesis begin");

            if (tx.outputs.size() < 2) {
                // cannot have less than 2 outputs for genesis
                return;
            }

            gs::tokenid tokenid(tx.txid.v);
            tokens.emplace(tokenid, tx);
            auto token_search = tokens.find(tokenid);
            assert(token_search != tokens.end()); // should never happen
            gs::slp_token & token = token_search->second;

            const gs::outpoint outpoint(tx.txid, 1);
            token.utxos.emplace(std::piecewise_construct,
                std::forward_as_tuple(outpoint),
                std::forward_as_tuple(outpoint, slp.qty)
            );
            utxo_to_tokenid.insert({ outpoint, tokenid });

            if (slp.has_mint_baton) {
                if (slp.mint_baton_vout < tx.outputs.size()) {
                    const gs::outpoint mint_baton_outpoint(tx.txid, slp.mint_baton_vout);
                    token.mint_baton_outpoint = mint_baton_outpoint;
                    utxo_to_tokenid.insert({ mint_baton_outpoint, tokenid });
                }
            }

            // spdlog::info("genesis end");
        }
        else if (tx.slp.type == gs::slp_transaction_type::mint) {
            // spdlog::info("mint begin");
            const auto slp = absl::get<gs::slp_transaction_mint>(tx.slp.slp_tx);
            auto token_search = tokens.find(tx.slp.tokenid);
            if (token_search == tokens.end()) {
                spdlog::warn("mint token not found");
                return;
            }
            gs::slp_token & token = token_search->second;

            if (! token.mint_baton_outpoint.has_value()) {
                spdlog::warn("mint no mint_baton_outpoint");
                return;
            }

            bool found = false;
            for (auto & input : tx.inputs) {
                if (token.mint_baton_outpoint.value() == input) {
                    found = true;
                }
            }

            if (! found) {
                spdlog::warn("mint not found");
                return;
            }

            token.transactions.insert({ tx.txid, tx });

            const gs::outpoint outpoint(tx.txid, 1);
            token.utxos.emplace(std::piecewise_construct,
                std::forward_as_tuple(outpoint),
                std::forward_as_tuple(outpoint, slp.qty)
            );
            utxo_to_tokenid.insert({ outpoint, tx.slp.tokenid });

            if (slp.has_mint_baton && slp.mint_baton_vout < tx.outputs.size()) {
                const gs::outpoint mint_baton_outpoint(tx.txid, slp.mint_baton_vout);
                token.mint_baton_outpoint = mint_baton_outpoint;
                token.utxos.emplace(std::piecewise_construct,
                    std::forward_as_tuple(mint_baton_outpoint),
                    std::forward_as_tuple(outpoint, mint_baton_outpoint)
                );
                utxo_to_tokenid.insert({ mint_baton_outpoint, tx.slp.tokenid });
            }

            // spdlog::info("mint end");
        }
        else if (tx.slp.type == gs::slp_transaction_type::send) {
            // spdlog::info("send begin");
            const auto slp = absl::get<gs::slp_transaction_send>(tx.slp.slp_tx);
            auto token_search = tokens.find(tx.slp.tokenid);
            if (token_search == tokens.end()) {
                spdlog::warn("send token not found");
                return;
            }
            gs::slp_token & token = token_search->second;

            absl::uint128 total_token_inputs = 0;
            for (auto & outpoint : tx.inputs) {
                auto utxo_search = token.utxos.find(outpoint);
                if (utxo_search == token.utxos.end()) {
                    continue;
                }

                total_token_inputs += utxo_search->second.amount;
            }

            absl::uint128 total_token_outputs = 0;
            for (const std::uint64_t amount : slp.amounts) {
                total_token_outputs += amount;
            }

            if (total_token_outputs > total_token_inputs) {
                //spdlog::warn("send outputs more than inputs ({}/{})", total_token_outputs, total_token_inputs);
                return;
            }

            for (std::size_t i=0; i<slp.amounts.size() && i<1+tx.outputs.size(); ++i) {
                const gs::outpoint outpoint(tx.txid, i+1);
                token.utxos.emplace(std::piecewise_construct,
                    std::forward_as_tuple(outpoint),
                    std::forward_as_tuple(outpoint, slp.amounts[i])
                );
                utxo_to_tokenid.insert({ outpoint, tx.slp.tokenid });
            }

            token.transactions.insert({ tx.txid, tx });

            // spdlog::info("send end");
        }
    }
};

}

#endif
