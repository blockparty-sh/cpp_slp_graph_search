#ifndef GS_TX_HPP
#define GS_TX_HPP

#include <string>
#include <vector>
#include "bhash.hpp"

namespace gs {

struct gs_tx
{
    gs::txid              txid;
    std::string           txdata;
    std::vector<gs::txid> inputs;

    gs_tx (
        gs::txid              txid,
        std::string           txdata,
        std::vector<gs::txid> inputs
    )
    : txid(txid)
    , txdata(txdata)
    , inputs(inputs)
    {}
};

}

#endif
