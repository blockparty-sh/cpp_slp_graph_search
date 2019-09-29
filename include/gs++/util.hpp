#ifndef GS_UTIL_HPP
#define GS_UTIL_HPP

#include <cstdint>
#include <algorithm>

namespace gs {

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
std::uint8_t extract_u32(Iterator & it)
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
std::uint8_t extract_i32(Iterator & it)
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
std::uint64_t extract_var_int (Iterator & it) 
{
    const std::uint64_t ret = extract_u8(it);

         if (ret  < 0xFD) return ret;
    else if (ret == 0xFD) return extract_u16(it);
    else if (ret == 0xFE) return extract_u32(it);
    else                  return extract_u64(it);
}


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

}

}


#endif
