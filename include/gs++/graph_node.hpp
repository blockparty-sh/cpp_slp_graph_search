#ifndef GS_GRAPH_NODE_HPP
#define GS_GRAPH_NODE_HPP

#include <vector>
#include <cstdint>
#include <gs++/bhash.hpp>

namespace gs {

struct graph_node
{
    std::vector<graph_node*>  inputs;
    std::vector<std::uint8_t> txdata;

    graph_node () {}

    graph_node (const std::vector<std::uint8_t>& txdata)
    : txdata(txdata)
    {}
};

}

#endif
