#ifndef GS_TRANSACTION_HPP
#define GS_TRANSACTION_HPP

#define ENABLE_BCH_PARSE_PRINTING

#include <vector>
#include <cstdint>
#include <algorithm>

#include <absl/types/variant.h>
#include <boost/format.hpp>

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

    transaction()
    : slp()
    {}

    template <typename BeginIterator, typename EndIterator>
    bool hydrate(
        BeginIterator&& begin_it,
        EndIterator&& end_it,
        const std::uint32_t height
    ) {
        constexpr std::uint64_t MAX_TX_SIZE = 1000000;
        constexpr std::uint64_t MAX_INPUTS  = MAX_TX_SIZE / 64; // TODO set better max
        constexpr std::uint64_t MAX_OUTPUTS = MAX_TX_SIZE; // TODO set better max
        constexpr std::uint64_t MAX_SCRIPT_SIZE = 1000; // TODO set better max

#ifdef ENABLE_BCH_PARSE_PRINTING
        #define CHECK_END(n) {    \
            if (it+n >= end_it) { \
                std::cerr << "CHECK_END\tline: " << __LINE__ << "\n";\
                return false;     \
            }                     \
        }

        #define DEBUG_PRINT(msg) {\
            std::cerr << msg << "\tline: " << __LINE__ << "\n";\
            std::cerr << "offset: " << (boost::format("%1$#x") % (it - begin_it)) << "\n";\
        }
#else
        #define CHECK_END(n) {    \
            if (it+n >= end_it) { \
                return false;     \
            }                     \
        }

        #define DEBUG_PRINT(msg) {\
        }
#endif

        auto it = begin_it;

        CHECK_END(4);
        this->version = gs::util::extract_i32(it);
        DEBUG_PRINT(this->version);

        CHECK_END(1+gs::util::var_int_additional_size(it));
        const std::uint64_t in_count { gs::util::extract_var_int(it) };
        DEBUG_PRINT(in_count);
        if (in_count >= MAX_INPUTS) {
            DEBUG_PRINT("in_count >= MAX_INPUTS");
            return false;
        }

        this->inputs.reserve(in_count);
        for (std::uint32_t in_i=0; in_i<in_count; ++in_i) {
            CHECK_END(32);
            gs::txid prev_tx_id;
            std::copy(it, it+32, reinterpret_cast<char*>(prev_tx_id.begin()));
            it+=32;
            DEBUG_PRINT(prev_tx_id.decompress(true));

            CHECK_END(4);
            const std::uint32_t prev_out_idx { gs::util::extract_u32(it) };
            DEBUG_PRINT(prev_out_idx);
            CHECK_END(1+gs::util::var_int_additional_size(it));
            const std::uint64_t script_len   { gs::util::extract_var_int(it) };
            DEBUG_PRINT(script_len);
            if (script_len >= MAX_SCRIPT_SIZE) {
                DEBUG_PRINT("len  >= MAX_SCRIPT_SIZE");
                return false;
            }
            
            CHECK_END(script_len);
            std::vector<std::uint8_t> sigscript;
            sigscript.reserve(script_len);
            std::copy(it, it+script_len, std::back_inserter(sigscript));
            it+=script_len;

            CHECK_END(4);
            const std::uint32_t sequence { gs::util::extract_u32(it) };
            DEBUG_PRINT(sequence);

            this->inputs.push_back(gs::outpoint(prev_tx_id, prev_out_idx));
        }

        CHECK_END(1+gs::util::var_int_additional_size(it));
        const std::uint64_t out_count { gs::util::extract_var_int(it) };
        DEBUG_PRINT(out_count);
        if (out_count >= MAX_OUTPUTS) {
            DEBUG_PRINT("out_count >= MAX_OUTPUTS");
            return false;
        }
        
        this->outputs.reserve(out_count);
        for (std::uint32_t out_i=0; out_i<out_count; ++out_i) {
            CHECK_END(8);
            const std::uint64_t value      { gs::util::extract_u64(it) };
            DEBUG_PRINT(value);

            CHECK_END(1+gs::util::var_int_additional_size(it));
            const std::uint64_t script_len { gs::util::extract_var_int(it) };
            DEBUG_PRINT(script_len);
            if (script_len >= MAX_SCRIPT_SIZE) {
                return false;
            }

            CHECK_END(script_len);
            gs::scriptpubkey scriptpubkey(script_len);
            std::copy(it, it+script_len, std::back_inserter(scriptpubkey.v));
            it+=script_len;

            this->outputs.push_back(gs::output({}, out_i, height, value, scriptpubkey));
        }

        CHECK_END(4-1); // minus 1 because +4 could be the end
        this->lock_time = gs::util::extract_u32(it);
        DEBUG_PRINT(this->lock_time);

        const auto tx_end_it = it;

        std::vector<std::uint8_t> serialized_tx;
        serialized_tx.reserve(tx_end_it - it);
        std::copy(begin_it, tx_end_it, std::back_inserter(serialized_tx));

        sha256(serialized_tx.data(), serialized_tx.size(), this->txid.v.data());
        sha256(this->txid.v.data(), this->txid.v.size(), this->txid.v.data());

        for (auto & m : this->outputs) {
            m.prev_tx_id = this->txid;
        }

        if (this->outputs.size() > 0) {
            if (this->outputs[0].is_op_return()) {
                this->slp = gs::slp_transaction(this->outputs[0].scriptpubkey);
            }
        }

        return true;
    }

    template <typename BeginIterator, typename EndIterator>
    bool hydrate(
        const BeginIterator&& begin_it,
        const EndIterator&& end_it,
        const std::uint32_t height
    ) {
        return hydrate(begin_it, end_it, height);
    }

    std::uint64_t output_slp_amount(const std::uint64_t vout) const
    {
        if      (slp.type == slp_transaction_type::send) {
            const auto & s = absl::get<gs::slp_transaction_send>(slp.slp_tx);

            if (vout > 0 && vout-1 < s.amounts.size()) {
                return s.amounts[vout-1];
            }
        }
        else if (slp.type == slp_transaction_type::mint) {
            const auto & s = absl::get<gs::slp_transaction_mint>(slp.slp_tx);
            if (vout == 1) {
                return s.qty;
            }
        }
        else if (slp.type == slp_transaction_type::genesis) {
            const auto & s = absl::get<gs::slp_transaction_genesis>(slp.slp_tx);
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
