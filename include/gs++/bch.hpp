#ifndef GS_BCH_HPP
#define GS_BCH_HPP

#include <vector>
#include <cstdint>
#include <boost/thread.hpp>
#include <absl/container/flat_hash_map.h>
#include <gs++/utxodb.hpp>
#include <gs++/slpdb.hpp>

namespace gs {

struct bch
{
    boost::shared_mutex lookup_mtx; // IMPORTANT: lookups/inserts must be guarded with the lookup_mtx
    gs::utxodb utxodb;
    gs::slpdb slpdb;

    bch()
    {}

    void process_block(
        const std::vector<std::uint8_t>& block_data,
        const bool save_rollback
    );

    void process_mempool_tx(
        const std::vector<std::uint8_t>& msg_data
    );

    void rollback();
};

}

#endif

