#ifndef GS_GRAPH_NODE_HPP
#define GS_GRAPH_NODE_HPP

#include <vector>
#include <string>
#include "graph_node.hpp"
#include "bhash.hpp"

struct graph_node
{
    bhash<btxid>             txid;
    std::vector<graph_node*> inputs;
    std::string              txdata;

    graph_node () {}

    graph_node (
        const bhash<btxid> & txid,
        const std::string  & txdata
    )
    : txid(txid)
    , txdata(txdata)
    {}
};

#endif
