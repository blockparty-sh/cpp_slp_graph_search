#ifndef GS_BLOCK_HPP
#define GS_BLOCK_HPP

#include <cstdint>
#include <cassert>
#include <vector>
#include <algorithm>
#include <iostream>

#include <gs++/transaction.hpp>
#include <gs++/bhash.hpp>
#include <gs++/util.hpp>

namespace gs {

struct block
{
    std::uint32_t version;
    gs::txid prev_block; // TODO detect re-org
    gs::txid merkle_root;
    std::uint32_t timestamp;
    std::uint32_t bits;
    std::uint32_t nonce;
    std::vector<gs::transaction> txs;


    template <typename BeginIterator, typename EndIterator>
    bool hydrate(
        BeginIterator&& begin_it,
        EndIterator&& end_it,
        bool slp_only=false
    ) {
#define ENABLE_BCH_PARSE_PRINTING
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
        this->version = gs::util::extract_u32(it);

        CHECK_END(32);
        std::copy(it, it+32, reinterpret_cast<char*>(this->prev_block.begin()));
        it+=32;

        CHECK_END(32);
        std::copy(it, it+32, reinterpret_cast<char*>(this->merkle_root.begin()));
        it+=32;

        CHECK_END(4);
        this->timestamp = gs::util::extract_u32(it);
        CHECK_END(4);
        this->bits      = gs::util::extract_u32(it);
        CHECK_END(4);
        this->nonce     = gs::util::extract_u32(it);

        CHECK_END(gs::util::var_int_additional_size(it));
        const std::uint64_t txn_count { gs::util::extract_var_int(it) };
        for (std::uint64_t i=0; i<txn_count; ++i) {
            CHECK_END(0);
            gs::transaction tx;
            const bool hydration_success = tx.hydrate(it, end_it);

            if (! hydration_success) {
                std::cerr << "wtf\n";
                return false;
            }

            if (! slp_only || tx.slp.type != gs::slp_transaction_type::invalid) {
                txs.push_back(tx);
            }

            it += tx.serialized.size();
        }

        return true;
    }

    void topological_sort();

    std::vector<std::uint8_t> serialize() const;
};

}

std::ostream & operator<<(std::ostream &os, const gs::block & block);

#endif
