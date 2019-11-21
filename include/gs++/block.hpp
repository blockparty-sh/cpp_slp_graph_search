#ifndef GS_BLOCK_HPP
#define GS_BLOCK_HPP

#include <cstdint>
#include <cassert>
#include <vector>
#include <algorithm>
#include <iostream>

#include <gs++/transaction.hpp>
#include <gs++/bhash.hpp>

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
        EndIterator&& end_it
    ) {
        // TODO we need to check that block data isnt malformed here
        // ie CHECK_END or something
        auto it = begin_it;
        this->version = gs::util::extract_u32(it);

        std::copy(it, it+32, reinterpret_cast<char*>(this->prev_block.begin()));
        it+=32;

        std::copy(it, it+32, reinterpret_cast<char*>(this->merkle_root.begin()));
        it+=32;

        this->timestamp = gs::util::extract_u32(it);
        this->bits      = gs::util::extract_u32(it);
        this->nonce     = gs::util::extract_u32(it);

        const std::uint64_t txn_count { gs::util::extract_var_int(it) };
        for (std::uint64_t i=0; i<txn_count; ++i) {
            gs::transaction tx;
            const bool hydration_success = tx.hydrate(it, end_it);

            if (! hydration_success) {
                std::cerr << "wtf\n";
                return false;
            }

            txs.push_back(tx);

            it += tx.serialized.size();
        }

        return true;
    }
};

}

std::ostream & operator<<(std::ostream &os, const gs::block & block);

#endif
