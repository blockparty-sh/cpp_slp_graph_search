#ifndef GS_BHASH_HPP
#define GS_BHASH_HPP

#include <string>
#include <array>
#include <cassert>
#include <absl/hash/internal/hash.h>
#include <gs++/util.hpp>


namespace gs {

// do not use this directly, instead use one of the typedefs below
template <typename Tag, unsigned Size = 32>
struct bhash
{
    std::array<std::uint8_t, Size> v;

    bhash()
    : v({ 0 })
    {}

    bhash(const std::string & v_)
    {
        assert(v.size() == Size*2);

        for (unsigned i=0; i<Size; ++i) {
            const char p1 = v_[(i<<1)+0];
            const char p2 = v_[(i<<1)+1];

            assert((p1 >= '0' && p1 <= '9') || (p1 >= 'a' && p1 <= 'f'));
            assert((p2 >= '0' && p2 <= '9') || (p2 >= 'a' && p2 <= 'f'));

            v[i] = ((p1 >= '0' && p1 <= '9' ? p1 - '0' : p1 - 'a' + 10) << 4)
                 +  (p2 >= '0' && p2 <= '9' ? p2 - '0' : p2 - 'a' + 10);
        }
    }

    constexpr auto size() -> decltype(v.size()) const
    { return v.size(); }

    auto data()  -> decltype(v.data())
    { return v.data(); }

    auto begin() -> decltype(v.begin())
    { return v.begin(); }

    auto end()   -> decltype(v.end())
    { return v.end(); }

    bool operator==(const bhash<Tag, Size> &o) const
    { return v == o.v; }

    bool operator!=(const bhash<Tag, Size> &o) const
    { return ! operator==(o); }

    template <typename H>
    friend H AbslHashValue(H h, const bhash<Tag, Size>& m)
    {
        return H::combine(std::move(h), m.v);
    }

    std::string decompress() const
    {
        return gs::util::decompress_hex(v);
    }
};

using txid    = bhash<struct btxid>;
using tokenid = bhash<struct btokenid>;

}

#endif
