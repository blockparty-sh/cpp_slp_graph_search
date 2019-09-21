#ifndef GS_TOKEN_DETAILS_HPP
#define GS_TOKEN_DETAILS_HPP

#include <absl/container/node_hash_map.h>
#include "graph_node.hpp"
#include "bhash.hpp"

namespace gs {

struct token_details
{
    gs::tokenid                               tokenid;
    absl::node_hash_map<gs::txid, graph_node> graph;

    token_details () {}

    token_details (gs::tokenid tokenid)
    : tokenid(tokenid)
    {}
};

}

#endif
