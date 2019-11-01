#ifndef GS_TRANSACTION_HPP
#define GS_TRANSACTION_HPP

#include <vector>
#include <cstdint>
#include <algorithm>

#include <gs++/bhash.hpp>
#include <gs++/util.hpp>
#include <gs++/output.hpp>
#include <gs++/slp_transaction.hpp>

#include <3rdparty/sha2.h>

namespace gs {

struct transaction
{
    gs::txid txid;
    std::int32_t  version;
    std::uint32_t lock_time;
    std::vector<gs::outpoint> inputs;
    std::vector<gs::output>   outputs;
    gs::slp_transaction slp;

    transaction() = default;

    template <typename Iterator>
    transaction(Iterator&& it, const std::uint32_t height)
    : slp{}
    {
        const auto begin_it = it;

        this->version  = gs::util::extract_i32(it);
        const std::uint64_t in_count { gs::util::extract_var_int(it) };

        this->inputs.reserve(in_count);
        for (std::uint32_t in_i=0; in_i<in_count; ++in_i) {
            gs::txid prev_tx_id;
            std::copy(it, it+32, reinterpret_cast<char*>(prev_tx_id.begin()));
            it+=32;

            const std::uint32_t prev_out_idx { gs::util::extract_u32(it) };
            const std::uint64_t script_len   { gs::util::extract_var_int(it) };
            
            std::vector<std::uint8_t> sigscript;
            sigscript.reserve(script_len);
            std::copy(it, it+script_len, std::back_inserter(sigscript));
            it+=script_len;

            const std::uint32_t sequence { gs::util::extract_u32(it) };

            this->inputs.push_back(gs::outpoint(prev_tx_id, prev_out_idx));
        }

        const std::uint64_t out_count { gs::util::extract_var_int(it) };
        
        this->outputs.reserve(out_count);
        for (std::uint32_t out_i=0; out_i<out_count; ++out_i) {
            const std::uint64_t value      { gs::util::extract_u64(it) };
            const std::uint64_t script_len { gs::util::extract_var_int(it) };

            gs::scriptpubkey scriptpubkey(script_len);
            std::copy(it, it+script_len, std::back_inserter(scriptpubkey.v));
            it+=script_len;

            this->outputs.push_back(gs::output({}, out_i, height, value, scriptpubkey));
        }

        this->lock_time = gs::util::extract_u32(it);

        const auto end_it = it;

        std::vector<std::uint8_t> serialized_tx;
        serialized_tx.reserve(end_it - begin_it);
        std::copy(begin_it, end_it, std::back_inserter(serialized_tx));

        sha256(serialized_tx.data(), serialized_tx.size(), this->txid.v.data());
        sha256(this->txid.v.data(), this->txid.v.size(), this->txid.v.data());
        // std::reverse(this->txid.begin(), this->txid.end());

        for (auto & m : this->outputs) {
            m.prev_tx_id = this->txid;
        }

        if (this->outputs.size() > 0) {
            if (this->outputs[0].is_op_return()) {
                this->slp = gs::slp_transaction(this->outputs[0].scriptpubkey);
            }
        }
    }

    template <typename Iterator>
    transaction(const Iterator&& it, const std::uint32_t height)
    : transaction(it, height)
    {}

    std::uint64_t output_slp_amount(const std::uint64_t vout) const
    {
        if      (slp.type == slp_transaction_type::send) {
            const auto & s = std::get<gs::slp_transaction_send>(slp.slp_tx);

            if (vout > 0 && vout-1 < s.amounts.size()) {
                return s.amounts[vout-1];
            }
        }
        else if (slp.type == slp_transaction_type::mint) {
            const auto & s = std::get<gs::slp_transaction_mint>(slp.slp_tx);
            if (vout == 1) {
                return s.qty;
            }
        }
        else if (slp.type == slp_transaction_type::genesis) {
            const auto & s = std::get<gs::slp_transaction_genesis>(slp.slp_tx);
            if (vout == 1) {
                return s.qty;
            }
        }
        else if (slp.type == slp_transaction_type::invalid) {
            return 0;
        }

        return 0;
    }
};

}

#endif
