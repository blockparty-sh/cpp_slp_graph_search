#ifndef GS_TOKEN_DETAILS_HPP
#define GS_TOKEN_DETAILS_HPP

#include <absl/container/node_hash_map.h>
#include "graph_node.hpp"
#include "bhash.hpp"


struct token_details
{
    bhash<btokenid>                               tokenid;
    absl::node_hash_map<bhash<btxid>, graph_node> graph;

    token_details () {}

    token_details (bhash<btokenid> tokenid)
    : tokenid(tokenid)
    {}
};

#endif
