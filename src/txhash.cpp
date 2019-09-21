#include <string>
#include <cassert>
#include <array>
#include <iostream>
#include <gs++/txhash.hpp>

auto txhash::data() -> decltype(v.data())
{ return v.data(); }

auto txhash::begin() -> decltype(v.begin())
{ return v.begin(); }

auto txhash::end() -> decltype(v.end())
{ return v.end(); }

bool txhash::operator==(const txhash &o) const
{ return v == o.v; }

bool txhash::operator!=(const txhash &o) const
{ return ! operator==(o); }

std::string txhash::decompress() const
{
    constexpr std::array<std::uint8_t, 16> chars = {
        '0', '1', '2', '3', '4', '5', '6', '7',
        '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'
    };

    std::string ret(64, '\0');
    for (unsigned i=0; i<32; ++i) {
        ret[(i<<1)+0] = (chars[static_cast<std::uint8_t>(v[i]) >> 4]);
        ret[(i<<1)+1] = (chars[static_cast<std::uint8_t>(v[i]) & 0x0F]);
    }

    return ret;
}
