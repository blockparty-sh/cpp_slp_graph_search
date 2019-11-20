#ifndef GS_TXGRAPH_HPP
#define GS_TXGRAPH_HPP

#include <string>
#include <vector>
#include <boost/thread.hpp>
#include <absl/container/flat_hash_set.h>
#include <absl/container/node_hash_map.h>
#include "graph_node.hpp"
#include "token_details.hpp"
#include "bhash.hpp"
#include "gs_tx.hpp"

namespace gs {

enum class graph_search_status
{
    OK,                // normal response
    NOT_FOUND,         // could be invalid slp token or we havent seen it yet
    NOT_IN_TOKENGRAPH, // error: if found it should be in tokengraph
};

struct txgraph
{
    absl::node_hash_map<gs::tokenid, token_details>  tokens;
    absl::node_hash_map<gs::txid,    token_details*> txid_to_token;
    boost::shared_mutex lookup_mtx; // IMPORTANT: tokens and txid_to_token must be guarded with the lookup_mtx

    txgraph()
    {}

    std::pair<graph_search_status, std::vector<std::string>>
    graph_search__ptr(const gs::txid lookup_txid);

    // void clear_token_data (const gs::tokenid tokenid);

    unsigned insert_token_data (
        const gs::tokenid & tokenid,
        const std::vector<gs_tx> & txs
    );

};

}

#endif
