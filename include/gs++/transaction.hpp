#ifndef GS_TRANSACTION_HPP
#define GS_TRANSACTION_HPP

#include <string>
#include <vector>
#include "bhash.hpp"

struct transaction
{
    bhash<btxid> txid;
    std::string txdata;
    std::vector<bhash<btxid>> inputs;

    transaction(
        bhash<btxid>              txid,
        std::string               txdata,
        std::vector<bhash<btxid>> inputs
    )
    : txid(txid)
    , txdata(txdata)
    , inputs(inputs)
    {}
};

#endif
