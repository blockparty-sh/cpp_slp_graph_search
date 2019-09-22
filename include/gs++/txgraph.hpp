#ifndef GS_TXGRAPH_HPP
#define GS_TXGRAPH_HPP

#include <string>
#include <mutex>
#include <shared_mutex>
#include <vector>
#include <absl/container/flat_hash_set.h>
#include <absl/container/node_hash_map.h>
#include "graph_node.hpp"
#include "token_details.hpp"
#include "bhash.hpp"
#include "transaction.hpp"

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
    std::shared_mutex lookup_mtx; // IMPORTANT: tokens and txid_to_token must be guarded with the lookup_mtx

    txgraph()
    {}

    // this is the meat
    void recursive_walk__ptr (
        const graph_node* node,
        absl::flat_hash_set<const graph_node*> & seen
    ) const;

    std::pair<graph_search_status, std::vector<std::string>>
    graph_search__ptr(const gs::txid lookup_txid);

    // void clear_token_data (const gs::tokenid tokenid);

    unsigned insert_token_data (
        const gs::tokenid & tokenid,
        const std::vector<transaction> & txs
    );

    // TODO save writes into buffer to prevent many tiny writes
    // should improve performance
    bool save_token_to_disk(const gs::tokenid tokenid);

    std::vector<transaction> load_token_from_disk(const gs::tokenid tokenid);
};

}

#endif
