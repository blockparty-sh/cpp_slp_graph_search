#ifndef GS_SCRIPTPUBKEY_HPP
#define GS_SCRIPTPUBKEY_HPP

#include <vector>
#include <cstdint>
#include <absl/hash/internal/hash.h>
#include <3rdparty/cashaddr/cashaddr.h>

namespace gs {

struct scriptpubkey
{
    std::vector<std::uint8_t> v;
    using value_type = decltype(v)::value_type;

    scriptpubkey()
    {}

    scriptpubkey(const std::vector<std::uint8_t> v_)
	: v(v_.begin(), v_.end())
    {}

    scriptpubkey(const std::string v_)
	: v(v_.begin(), v_.end())
    {}

    scriptpubkey(const std::size_t size)
    {
        v.reserve(size);
    }

    auto size() -> decltype(v.size()) const
    { return v.size(); }

    auto data() -> decltype(v.data())
    { return v.data(); }

    auto begin() -> decltype(v.begin())
    { return v.begin(); }

    auto end() -> decltype(v.end())
    { return v.end(); }

    bool operator==(const scriptpubkey& o) const
    { return v == o.v; }

    bool operator!=(const scriptpubkey& o) const
    { return ! operator==(o); }

    template <typename H>
    friend H AbslHashValue(H h, const scriptpubkey& m)
    {
        return H::combine(std::move(h), m.v);
    }

    // TODO the return values need to be cashaddr on client?
    std::pair<std::string, std::vector<uint8_t>> to_address() const
    {
        // P2PKH
        if (v.size() > 5 
         && v[0] == 0x76 // OP_DUP
         && v[1] == 0xA9 // OP_HASH160
         && v.size() == 3u+v[2]+2u // scriptpubkey[2] holds length of sig
        ) { 
            return { "P2PKH", std::vector<uint8_t>(v.begin()+3, v.end()-2) };
        }
     
        // P2SH
        else
        if (v.size() == 23
         && v[0]  == 0xA9 // OP_HASH160
         && v[22] == 0x87 // OP_EQUAL
        ) { 
            return { "P2SH", std::vector<uint8_t>(v.begin()+1, v.begin()+21) };
        }   
     
        return {};
    }

    std::string to_cashaddr(std::string prefix) const {
        auto address(to_address());
        std::vector<uint8_t> data;
        uint8_t versionByte = getTypeBits(address.first) + getHashSizeBits(address.second);
        data.push_back(versionByte);
        std::copy(address.second.begin(), address.second.end(), std::back_inserter(data));
        return cashaddr::Encode(prefix, convertBits(data, 8, 5));
    }

    static scriptpubkey from_cashaddr(std::string cashaddr) {
        std::pair<std::string, std::vector<uint8_t>> decoded = cashaddr::Decode(cashaddr);
        if (decoded.first == std::string()) {
            return scriptpubkey();
        }

        const auto & hash = convertBits(decoded.second, 5, 8, true);

        switch (hash[0] & 120) {
            // P2PKH
            case 0: {
                std::vector<uint8_t> res;
                res.push_back(0x76); res.push_back(0xA9); res.push_back(hash.size() - 1);
                std::copy(hash.begin() + 1, hash.end(), std::back_inserter(res));
                res.push_back(0x88); res.push_back(0xAC);
                return res;
            };
            // P2SH
            case 8: {
                std::vector<uint8_t> res;
                res.push_back(0xA9); res.push_back(hash.size() - 1);
                std::copy(hash.begin() + 1, hash.end(), std::back_inserter(res));
                res.push_back(0x87);
                return res;
            };
        }
        return scriptpubkey();
    }

    static std::vector<uint8_t> convertBits(std::vector<uint8_t> & data, uint8_t from, uint8_t to, bool strictMode = false) {
        const auto length = strictMode
            ? floor(1.f * data.size() * from / to)
            : ceil(1.f * data.size() * from / to);

        const uint8_t mask = (1 << to) - 1;
        std::vector<std::uint8_t> result(length);
        uint8_t index = 0;
        uint32_t accumulator = 0;
        uint8_t bits = 0;
        for (size_t i = 0; i < data.size(); ++i) {
            const uint8_t & value = data[i];
            assert(0 <= value && (value >> from) == 0);
            accumulator = (accumulator << from) | value;
            bits += from;
            while (bits >= to) {
                bits -= to;
                result[index] = (accumulator >> bits) & mask;
                ++index;
            }
        }
        if (!strictMode) {
            if (bits > 0) {
                result[index] = (accumulator << (to - bits)) & mask;
                ++index;
            }
        } else {
            assert(
                bits < from && ((accumulator << (to - bits)) & mask) == 0
            );
        }
        return result;
    }

    static std::uint8_t getTypeBits(std::string type) {
        if (type == "P2PKH")
            return 0;
        if (type == "P2SH")
            return 8;

        assert(false);
        return 0;
    }

    template <typename Container>
    static std::uint16_t getHashSizeBits(Container hash) {
        switch (hash.size() * 8) {
            case 160:
                return 0;
            case 192:
                return 1;
            case 224:
                return 2;
            case 256:
                return 3;
            case 320:
                return 4;
            case 384:
                return 5;
            case 448:
                return 6;
            case 512:
                return 7;
        }

        assert(false);
        return 0;
    }
};

}

#endif
