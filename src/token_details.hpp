#ifndef GS_TOKEN_DETAILS_HPP
#define GS_TOKEN_DETAILS_HPP

#include <absl/container/node_hash_set.h>
#include "graph_node.hpp"
#include "txhash.hpp"


struct token_details
{
    txhash                                  tokenid;
    absl::node_hash_map<txhash, graph_node> graph;

    token_details () {}

    token_details (txhash tokenid)
    : tokenid(tokenid)
    {}
};

#endif
