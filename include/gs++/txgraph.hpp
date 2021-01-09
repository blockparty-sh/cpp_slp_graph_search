#ifndef GS_TXGRAPH_HPP
#define GS_TXGRAPH_HPP

#include <string>
#include <vector>
#include <boost/thread.hpp>
#include <absl/container/flat_hash_set.h>
#include <absl/container/node_hash_map.h>
#include <gs++/transaction.hpp>
#include <gs++/graph_node.hpp>
#include <gs++/token_details.hpp>
#include <gs++/bhash.hpp>

namespace gs {

enum class graph_search_status
{
    OK,                // normal response
    NOT_FOUND,         // could be invalid slp token or we havent seen it yet
    NOT_IN_TOKENGRAPH, // error: if found it should be in tokengraph
};

using graph_search_response = std::pair<graph_search_status, std::vector<std::vector<std::uint8_t>>>;

struct txgraph
{
    absl::node_hash_map<gs::tokenid, token_details>  tokens;
    absl::node_hash_map<gs::txid,    token_details*> txid_to_token;
    boost::shared_mutex lookup_mtx; // IMPORTANT: tokens and txid_to_token must be guarded with the lookup_mtx

    txgraph()
    {}

    void clear();

    bool build_exclusion_set(
        const gs::txid lookup_txid,
        absl::flat_hash_set<const graph_node*>& seen
    );

    // this will modify the exclusion set so keep in mind
    graph_search_response graph_search__ptr(
        const gs::txid lookup_txid,
        absl::flat_hash_set<const graph_node*>& seen
    );

    unsigned insert_token_data (
        const gs::tokenid & tokenid,
        const std::vector<gs::transaction> & txs
    );

};

}

#endif
