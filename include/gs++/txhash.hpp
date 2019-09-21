#ifndef GS_TXHASH_HPP
#define GS_TXHASH_HPP

#include <string>
#include <array>
#include <cassert>
#include <absl/hash/internal/hash.h>

struct txhash
{
    std::array<std::uint8_t, 32> v;

    txhash()
    : v({ 0 })
    {}

    txhash(const std::string & v_)
    {
        assert(v.size() == 64);

        for (unsigned i=0; i<32; ++i) {
            const char p1 = v_[(i<<1)+0];
            const char p2 = v_[(i<<1)+1];

            v[i] = ((p1 >= '0' && p1 <= '9' ? p1 - '0' : p1 - 'a' + 10) << 4)
                 +  (p2 >= '0' && p2 <= '9' ? p2 - '0' : p2 - 'a' + 10);
        }
    }

    constexpr std::size_t size() const
    { return v.size(); }

    auto data()  -> decltype(v.data());
    auto begin() -> decltype(v.begin());
    auto end()   -> decltype(v.end());

    bool operator==(const txhash &o) const;

    bool operator!=(const txhash &o) const;

    template <typename H>
    friend H AbslHashValue(H h, const txhash& m)
    {
        return H::combine(std::move(h), m.v);
    }

    std::string decompress() const;
};

#endif
