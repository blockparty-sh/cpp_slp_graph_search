#ifndef GS_TRANSACTION_HPP
#define GS_TRANSACTION_HPP

#include <string>
#include <vector>
#include "bhash.hpp"

namespace gs {

struct transaction
{
    gs::txid              txid;
    std::string           txdata;
    std::vector<gs::txid> inputs;

    transaction (
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
