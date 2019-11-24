#include <iostream>
#include <algorithm>
#include <sstream>
#include <gs++/block.hpp>


namespace gs {


void block::topological_sort()
{
    txs = gs::util::topological_sort(txs);
}

std::vector<std::uint8_t> block::serialize() const
{
    std::stringstream ss;

    ss.write(reinterpret_cast<const char *>(&version), sizeof(std::uint32_t));
    ss.write(reinterpret_cast<const char *>(prev_block.data()), prev_block.size());
    ss.write(reinterpret_cast<const char *>(merkle_root.data()), merkle_root.size());
    ss.write(reinterpret_cast<const char *>(&timestamp), sizeof(std::uint32_t));
    ss.write(reinterpret_cast<const char *>(&bits), sizeof(std::uint32_t));
    ss.write(reinterpret_cast<const char *>(&nonce), sizeof(std::uint32_t));

    const std::vector<std::uint8_t> varint_tx_length = gs::util::num_to_var_int(txs.size());
    ss.write(reinterpret_cast<const char *>(varint_tx_length.data()), varint_tx_length.size());

    for (auto & tx : txs) {
        ss.write(reinterpret_cast<const char *>(tx.serialized.data()), tx.serialized.size());
    }

    const std::string str = ss.str();
    const std::vector<std::uint8_t> ret(str.begin(), str.end());

    return ret;
}

}

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
