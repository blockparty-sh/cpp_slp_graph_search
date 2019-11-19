#include <gs++/block.hpp>
#include <iostream>

std::ostream & operator<<(std::ostream &os, const gs::block & block)
{
    os
        << block.version << "\n"
        << block.prev_block.decompress(true) << "\n"
        << block.merkle_root.decompress(true) << "\n"
        << block.timestamp << "\n"
        << block.bits << "\n"
        << block.nonce << "\n";

    return os;
}
