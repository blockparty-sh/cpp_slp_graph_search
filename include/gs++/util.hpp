#ifndef GS_UTIL_HPP
#define GS_UTIL_HPP

#include <cstdint>
#include <algorithm>
#include <vector>
#include <cassert>
#include <array>
#include <string>
#include <iostream>

// You must include gs++/transaction.hpp before including this file

namespace gs {

struct transaction;

namespace util {

template <typename Iterator>
std::uint8_t extract_u8(Iterator & it)
{
    const std::uint8_t ret = *it;
    ++it;
    return ret;
}

template <typename Iterator>
std::uint16_t extract_u16(Iterator & it)
{
    std::uint16_t ret;
    std::copy(it, it+2, reinterpret_cast<std::uint8_t*>(&ret));
    it+=2;
    return ret;
}

template <typename Iterator>
std::uint32_t extract_u32(Iterator & it)
{
    std::uint32_t ret;
    std::copy(it, it+4, reinterpret_cast<std::uint8_t*>(&ret));
    it+=4;
    return ret;
}
template <typename Iterator>
std::uint64_t extract_u64(Iterator & it)
{
    std::uint64_t ret;
    std::copy(it, it+8, reinterpret_cast<std::uint8_t*>(&ret));
    it+=8;
    return ret;
}

template <typename Iterator>
std::int8_t extract_i8(Iterator & it)
{
    const std::int8_t ret = *it;
    ++it;
    return ret;
}

template <typename Iterator>
std::int16_t extract_i16(Iterator & it)
{
    std::uint16_t ret;
    std::copy(it, it+2, reinterpret_cast<std::uint8_t*>(&ret));
    it+=2;
    return ret;
}

template <typename Iterator>
std::uint32_t extract_i32(Iterator & it)
{
    std::int32_t ret;
    std::copy(it, it+4, reinterpret_cast<std::uint8_t*>(&ret));
    it+=4;
    return ret;
}
template <typename Iterator>
std::int64_t extract_i64(Iterator & it)
{
    std::int64_t ret;
    std::copy(it, it+8, reinterpret_cast<std::uint8_t*>(&ret));
    it+=8;
    return ret;
}

template <typename Iterator>
std::size_t var_int_additional_size(const Iterator & it)
{
        const std::uint8_t v = *it;

         if (v  < 0xFD) return 0;
    else if (v == 0xFD) return 2;
    else if (v == 0xFE) return 4;
    else                return 8;
}

template <typename Iterator>
std::uint64_t extract_var_int (Iterator & it) 
{
    const std::uint64_t ret = extract_u8(it);

         if (ret  < 0xFD) return ret;
    else if (ret == 0xFD) return extract_u16(it);
    else if (ret == 0xFE) return extract_u32(it);
    else                  return extract_u64(it);
}

std::vector<std::uint8_t> num_to_var_int(const std::uint64_t n);

template <typename Container>
std::string decompress_hex(const Container& v)
{
    constexpr std::array<std::uint8_t, 16> chars = {
        '0', '1', '2', '3', '4', '5', '6', '7',
        '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'
    };

    std::string ret(v.size()*2, '\0');
    for (unsigned i=0; i<v.size(); ++i) {
        ret[(i<<1)+0] = chars[v[i] >> 4];
        ret[(i<<1)+1] = chars[v[i] & 0x0F];
    }

    return ret;
}


template <typename Container>
std::vector<std::uint8_t> compress_hex(const Container& v_)
{
    std::vector<std::uint8_t> ret(v_.size() / 2);

    for (unsigned i=0; i<ret.size(); ++i) {
        const char p1 = v_[(i<<1)+0];
        const char p2 = v_[(i<<1)+1];

#ifndef NDEBUG
        if ((p1 >= '0' && p1 <= '9') || (p1 >= 'a' && p1 <= 'f')) {
            std::cerr << "compress_hex p1 out of range (DEBUG MODE IS ON)\n";
        }
        if ((p2 >= '0' && p2 <= '9') || (p2 >= 'a' && p2 <= 'f')) {
            std::cerr << "compress_hex p2 out of range (DEBUG MODE IS ON)\n";
        }
#endif

        ret[i] = ((p1 <= '9' ? p1 - '0' : p1 - 'a' + 10) << 4)
               +  (p2 <= '9' ? p2 - '0' : p2 - 'a' + 10);
    }

    return ret;
}

std::vector<gs::transaction> topological_sort(
    const std::vector<gs::transaction>& tx_list
);

}

}


#endif
