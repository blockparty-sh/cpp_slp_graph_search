#ifndef GS_TRANSACTION_HPP
#define GS_TRANSACTION_HPP

#include <string>
#include <vector>
#include "txhash.hpp"

struct transaction
{
    txhash txid;
    std::string txdata;
    std::vector<txhash> inputs;

    transaction(
        txhash txid,
        std::string txdata,
        std::vector<txhash> inputs
    )
    : txid(txid)
    , txdata(txdata)
    , inputs(inputs)
    {}
};

#endif
