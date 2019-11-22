#include <gs++/block.hpp>
#include <iostream>

std::ostream & operator<<(std::ostream &os, const gs::block & block)
{
    os
        << "version:     " << block.version << "\n"
        << "prev_block:  " << block.prev_block.decompress(true) << "\n"
        << "merkle_root: " << block.merkle_root.decompress(true) << "\n"
        << "timestamp:   " << block.timestamp << "\n"
        << "bits:        " << block.bits << "\n"
        << "nonce:       " << block.nonce << "\n";

    for (const auto & tx : block.txs) {
        os << "--------------------------------------------------------------------------------\n";
        os << tx;
    }

    return os;
}
