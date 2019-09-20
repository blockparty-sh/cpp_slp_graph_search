#include <string>
#include <cassert>
#include <array>
#include <iostream>
#include <gs++/txhash.hpp>

txhash compress_txhash(const std::string hex)
{
    assert(hex.size() == 64);

    std::string ret;
    ret.reserve(32);
    for (unsigned i=0; i<64; i+=2) {
        const char p1 = hex[i+0];
        const char p2 = hex[i+1];

        ret.push_back(
            ((p1 >= '0' && p1 <= '9' ? p1 - '0' : p1 - 'a' + 10) << 4) +
             (p2 >= '0' && p2 <= '9' ? p2 - '0' : p2 - 'a' + 10)
        );
    }

    return ret;
}

txhash decompress_txhash(const txhash hash)
{
    assert(hash.size() == 32);

    constexpr std::array<std::uint8_t, 16> chars = {
        '0', '1', '2', '3', '4', '5', '6', '7',
        '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'
    };

    std::string ret;
    ret.reserve(64);
    for (unsigned i=0; i<32; ++i) {
        ret.push_back(chars[static_cast<std::uint8_t>(hash[i]) >> 4]);
        ret.push_back(chars[static_cast<std::uint8_t>(hash[i]) & 0x0F]);
    }

    return ret;
}
