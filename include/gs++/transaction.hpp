#ifndef GS_TRANSACTION_HPP
#define GS_TRANSACTION_HPP

#include <vector>
#include <cstdint>
#include <algorithm>

#include <gs++/bhash.hpp>
#include <gs++/util.hpp>
#include <gs++/output.hpp>

#include <3rdparty/picosha2.h>

namespace gs {

struct transaction
{
    gs::txid txid;
    std::int32_t  version;
    std::uint32_t lock_time;
    std::vector<gs::outpoint> inputs;
    std::vector<gs::output>   outputs;


    template <typename Iterator>
    transaction(Iterator&& it, const std::uint32_t height)
    {
        const auto begin_it = it;

        this->version  = gs::util::extract_i32(it);
        const std::uint64_t in_count { gs::util::extract_var_int(it) };

        this->inputs.reserve(in_count);
        for (std::uint32_t in_i=0; in_i<in_count; ++in_i) {
            gs::txid prev_tx_id;
            std::reverse_copy(it, it+32, reinterpret_cast<char*>(prev_tx_id.begin()));
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

            gs::pk_script pk_script(script_len);
            std::copy(it, it+script_len, std::back_inserter(pk_script.v));
            it+=script_len;

            // skip OP_RETURN
            if (pk_script.v[0] == 0x6a) {
                continue;
            }
            this->outputs.push_back(gs::output({}, out_i, height, value, pk_script));
        }

        this->lock_time = gs::util::extract_u32(it);

        const auto end_it = it;

        std::vector<std::uint8_t> serialized_tx;
        serialized_tx.reserve(end_it - begin_it);
        std::copy(begin_it, end_it, std::back_inserter(serialized_tx));

        picosha2::hash256(serialized_tx, this->txid.v);
        picosha2::hash256(this->txid.v, this->txid.v);
        std::reverse(this->txid.begin(), this->txid.end());

        for (auto & m : this->outputs) {
            m.prev_tx_id = this->txid;
        }
    }

    template <typename Iterator>
    transaction(Iterator&& it)
    : transaction(it, 0) {}
};

}

#endif
