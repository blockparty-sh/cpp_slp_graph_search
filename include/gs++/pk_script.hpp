#ifndef GS_PK_SCRIPT_HPP
#define GS_PK_SCRIPT_HPP

#include <vector>
#include <cstdint>
#include <absl/hash/internal/hash.h>

namespace gs {

struct pk_script
{
    std::vector<std::uint8_t> v;
    using value_type = decltype(v)::value_type;

    pk_script()
    {}

    pk_script(const std::string v_)
    : v(v_.begin(), v_.end())
    {}

    pk_script(const std::size_t size)
    {
        v.reserve(size);
    }

    constexpr auto size() -> decltype(v.size()) const
    { return v.size(); }

    auto data()  -> decltype(v.data())
    { return v.data(); }

    auto begin() -> decltype(v.begin())
    { return v.begin(); }

    auto end()   -> decltype(v.end())
    { return v.end(); }

    bool operator==(const pk_script& o) const
    { return v == o.v; }

    bool operator!=(const pk_script& o) const
    { return ! operator==(o); }

    template <typename H>
    friend H AbslHashValue(H h, const pk_script& m)
    {
        return H::combine(std::move(h), m.v);
    }

    // TODO the return values need to be cashaddr on client?
    std::string to_address()
    {
        // P2PKH
        if (v.size() > 5 
         && v[0] == 0x76 // OP_DUP
         && v[1] == 0xA9 // OP_HASH160
         && v.size() == 3+v[2]+2 // pk_script[2] holds length of sig
        ) { 
            return std::string(v.begin()+3, v.end()-2);
        }   
     
        // P2SH
        else
        if (v.size() == 23
         && v[0]  == 0xA9 // OP_HASH160
         && v[22] == 0x87 // OP_EQUAL
        ) { 
            return std::string(v.begin()+1, v.begin()+21);
        }   
     
        return "";
    }
};

}

#endif
