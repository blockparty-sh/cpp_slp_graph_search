#ifndef GS_SLP_TRANSACTION_HPP
#define GS_SLP_TRANSACTION_HPP

#include <string>
#include <cstdint>
#include <vector>
#include <iostream>
#include <absl/types/variant.h>
#include <gs++/bhash.hpp>
#include <gs++/util.hpp>
#include <gs++/scriptpubkey.hpp>

namespace gs {

struct slp_transaction_invalid
{};

struct slp_transaction_genesis
{
    std::string   ticker;
    std::string   name;
    std::string   document_uri;
    std::string   document_hash;
    std::uint32_t decimals;
    bool          has_mint_baton;
    std::uint32_t mint_baton_vout;
    std::uint64_t qty;

    slp_transaction_genesis(
        const std::string   ticker,
        const std::string   name,
        const std::string   document_uri,
        const std::string   document_hash,
        const std::uint32_t decimals,
        const bool          has_mint_baton,
        const std::uint32_t mint_baton_vout,
        const std::uint64_t qty
    )
    : ticker(ticker)
    , name(name)
    , document_uri(document_uri)
    , document_hash(document_hash)
    , decimals(decimals)
    , has_mint_baton(has_mint_baton)
    , mint_baton_vout(mint_baton_vout)
    , qty(qty)
    {}
};

struct slp_transaction_mint
{
    gs::tokenid   tokenid;
    bool          has_mint_baton;
    std::uint32_t mint_baton_vout;
    std::uint64_t qty;


    slp_transaction_mint(
        const gs::tokenid   tokenid,
        const bool          has_mint_baton,
        const std::uint32_t mint_baton_vout,
        const std::uint64_t qty
    )
    : tokenid(tokenid)
    , has_mint_baton(has_mint_baton)
    , mint_baton_vout(mint_baton_vout)
    , qty(qty)
    {}
};

struct slp_transaction_send
{
    gs::tokenid                tokenid;
    std::vector<std::uint64_t> amounts;

    slp_transaction_send(
        const gs::tokenid                tokenid,
        const std::vector<std::uint64_t> amounts
    )
    : tokenid(tokenid)
    , amounts(amounts)
    {}
};

enum class slp_transaction_type {
    invalid,
    genesis,
    mint,
    send
};

struct slp_transaction
{
    absl::variant<
        slp_transaction_invalid,
        slp_transaction_genesis,
        slp_transaction_mint,
        slp_transaction_send
    > slp_tx;
    slp_transaction_type type;

    slp_transaction()
    : type(slp_transaction_type::invalid)
    , slp_tx(slp_transaction_invalid{})
    {}

    slp_transaction(const slp_transaction& o)
    : type(slp_transaction_type::invalid)
    , slp_tx(slp_transaction_invalid{})
    {}

    slp_transaction(const slp_transaction_genesis slp_tx)
    : type(slp_transaction_type::genesis)
    , slp_tx(slp_tx)
    {}

    slp_transaction(const slp_transaction_mint slp_tx)
    : type(slp_transaction_type::mint)
    , slp_tx(slp_tx)
    {}

    slp_transaction(const slp_transaction_send slp_tx)
    : type(slp_transaction_type::send)
    , slp_tx(slp_tx)
    {}


    slp_transaction(
        const gs::scriptpubkey & scriptpubkey
    )
    : type(slp_transaction_type::invalid)
    , slp_tx(slp_transaction_invalid{})
    {
#ifdef ENABLE_SLP_PARSE_ERROR_PRINTING
        #define PARSE_CHECK(cond, msg) ({\
            if (cond) { \
                this->type = slp_transaction_type::invalid;\
                this->slp_tx = slp_transaction_invalid{};\
                std::cerr << msg << "\tline: " << __LINE__ << "\n";\
                return;\
            }\
        })
#else
        #define PARSE_CHECK(cond, msg) ({\
            if (cond) { \
                this->type = slp_transaction_type::invalid;\
                this->slp_tx = slp_transaction_invalid{};\
                return;\
            }\
        })
#endif

        PARSE_CHECK(scriptpubkey.v.empty(), "scriptpubkey cannot be empty");

        auto it = scriptpubkey.v.begin() + 1;

        // success, value
        auto extract_pushdata = [&it, &scriptpubkey]()
        -> std::pair<bool, std::uint64_t>
        {
            const std::uint8_t cnt = gs::util::extract_u8(it);

            if (cnt < 0x4C) {
                if (it+cnt > scriptpubkey.v.end()) {
                    return { false, 0 };
                }

                // OP_0 not allowed
                if (cnt == 0) {
                    return { false, 0 };
                }

                return { true, cnt };
            }
            else if (cnt == 0x4C) {
                if (it+1 >= scriptpubkey.v.end()) {
                    return { false, 0 };
                }
                return { true, gs::util::extract_u8(it) };
            }
            else if (cnt == 0x4D) {
                if (it+2 >= scriptpubkey.v.end()) {
                    return { false, 0 };
                }
                return { true, gs::util::extract_u16(it) };
            }
            else if (cnt == 0x4E) {
                if (it+4 >= scriptpubkey.v.end()) {
                    return { false, 0 };
                }
                return { true, gs::util::extract_u32(it) };
            }

            return { false, 0 };
        };

        auto extract_string = [&it, &scriptpubkey](const std::size_t len)
        -> std::pair<bool, std::string>
        {
            if (it+len > scriptpubkey.v.end()) {
                return { false, "" };
            }

            const std::string ret(it, it+len);
            it += len;

            return { true, ret };
        };

        auto string_to_number = [](const std::string & s)
        -> std::pair<bool, std::uint64_t>
        {
            auto sit = s.begin();
            if (s.size() == 1) return { true, gs::util::extract_u8(sit)  };
            if (s.size() == 2) return { true, gs::util::extract_u16(sit) };
            if (s.size() == 4) return { true, gs::util::extract_u32(sit) };
            if (s.size() == 8) return { true, gs::util::extract_u64(sit) };
            else return { false, 0 };
        };

        auto check_valid_token_id = [](const std::string & tokenid) -> bool {
            return tokenid.size() == 32;
        };

        std::vector<std::string> chunks;
        while (true) {
            const std::pair<bool, std::uint64_t> len = extract_pushdata();
            if (! len.first) {
                break;
            }

            const std::pair<bool, std::string> data = extract_string(len.second);
            PARSE_CHECK(! data.first, "pushdata data extraction failed");

            chunks.emplace_back(data.second);

            const std::string decompressed = gs::util::decompress_hex(data.second);
            // std::cout << "chunk: (" << decompressed.size() << ") " << decompressed << std::endl;
        }

        PARSE_CHECK(chunks.empty(), "chunks empty");

        auto cit = chunks.begin();

        #define CHECK_NEXT() ({\
            ++cit;\
            PARSE_CHECK(cit == chunks.end(), "parsing ended early");\
        })


        // lokad id
        {
            using namespace std::string_literals;
            PARSE_CHECK(*cit != "SLP\0"s, "SLP not in first chunk");
            CHECK_NEXT();
        }

        // token type
        std::uint64_t token_type = 0;
        {
            std::string token_type_str = *cit;
            std::reverse(token_type_str.begin(), token_type_str.end());
            PARSE_CHECK(token_type_str.size() != 1 && token_type_str.size() != 2,
                "token_type string length must be 1 or 2");

            const std::pair<bool, std::uint64_t> token_type_check = string_to_number(token_type_str);
            PARSE_CHECK(! token_type_check.first, "token_type extraction failed");

            token_type = token_type_check.second;
            PARSE_CHECK(token_type != 1, "token_type not equal to 1");

            CHECK_NEXT();
        }

        // action type
        if (*cit == "GENESIS") {
            PARSE_CHECK(chunks.size() != 10, "wrong number of chunks");
            CHECK_NEXT();

            const std::string ticker = *cit;
            CHECK_NEXT();

            const std::string name = *cit;
            CHECK_NEXT();

            const std::string document_uri = *cit;
            CHECK_NEXT();

            const std::string document_hash = *cit;
            PARSE_CHECK(! (document_hash.size() == 0 || document_hash.size() == 32),
                "document_hash must be size 0 or 32");
            CHECK_NEXT();


            std::uint64_t decimals = 0;
            {
                const std::string decimals_str = *cit;
                PARSE_CHECK(decimals_str.size() != 1, "decimals string length must be 1");

                const std::pair<bool, std::uint64_t> decimals_check {
                    string_to_number(decimals_str)
                };
                PARSE_CHECK(! decimals_check.first, "decimals parse failed");

                decimals = decimals_check.second;
                PARSE_CHECK(decimals > 9, "decimals bigger than 9");
                CHECK_NEXT();
            }

            bool has_mint_baton = false;
            std::uint32_t mint_baton_vout = 0;
            {
                const std::string mint_baton_vout_str = *cit;
                PARSE_CHECK(mint_baton_vout_str.size() >= 2, "mint_baton_vout string length must be 0 or 1");
                if (mint_baton_vout_str != "") {
                    const std::pair<bool, std::uint64_t> mint_baton_vout_check {
                        string_to_number(mint_baton_vout_str)
                    };
                    PARSE_CHECK(! mint_baton_vout_check.first, "mint_baton_vout parse failed");

                    mint_baton_vout = mint_baton_vout_check.second;
                    PARSE_CHECK(mint_baton_vout < 2, "mint_baton_vout must be at least 2");
                }
                CHECK_NEXT();
            }

            std::uint64_t qty = 0;
            {
                std::string initial_qty_str = *cit;
                std::reverse(initial_qty_str.begin(), initial_qty_str.end());

                const std::pair<bool, std::uint64_t> initial_qty_check {
                    string_to_number(initial_qty_str)
                };
                PARSE_CHECK(! initial_qty_check.first, "initial_qty parse failed");
                qty = initial_qty_check.second;
            }

            this->type = slp_transaction_type::genesis;
            this->slp_tx = slp_transaction_genesis(
                ticker,
                name,
                document_uri,
                document_hash,
                decimals,
                has_mint_baton,
                mint_baton_vout,
                qty
            );
        } else if(*cit == "MINT") {
            PARSE_CHECK(chunks.size() != 6, "wrong number of chunks");
            CHECK_NEXT();

            gs::tokenid tokenid;
            {
                const std::string tokenid_str = *cit;
                PARSE_CHECK(! check_valid_token_id(tokenid_str), "tokenid invalid size");
                tokenid = gs::tokenid(tokenid_str);
                CHECK_NEXT();
            }

            bool has_mint_baton = false;
            std::uint32_t mint_baton_vout = 0;
            {
                const std::string mint_baton_vout_str = *cit;
                if (mint_baton_vout_str != "") {
                    has_mint_baton = true;
                    const std::pair<bool, std::uint64_t> mint_baton_vout_check {
                        string_to_number(mint_baton_vout_str)
                    };
                    PARSE_CHECK(! mint_baton_vout_check.first, "mint_baton_vout parse failed");

                    mint_baton_vout = mint_baton_vout_check.second;
                    PARSE_CHECK(mint_baton_vout < 2, "mint_baton_vout must be at least 2");
                    PARSE_CHECK(mint_baton_vout > 0xFF, "mint_baton_vout must be below 0xFF");
                }
                CHECK_NEXT();
            }

            std::uint64_t additional_qty = 0;
            {
                std::string additional_qty_str = *cit;
                std::reverse(additional_qty_str.begin(), additional_qty_str.end());

                const std::pair<bool, std::uint64_t> additional_qty_check {
                    string_to_number(additional_qty_str)
                };
                PARSE_CHECK(! additional_qty_check.first, "additional_token_quantity parse failed");

                additional_qty = additional_qty_check.second;
            }

            this->type = slp_transaction_type::mint;
            this->slp_tx = slp_transaction_mint(
                tokenid,
                has_mint_baton,
                mint_baton_vout,
                additional_qty
            );
        } else if(*cit == "SEND") {
            CHECK_NEXT();

            gs::tokenid tokenid;
            {
                const std::string tokenid_str = *cit;
                PARSE_CHECK(! check_valid_token_id(tokenid_str), "tokenid invalid size");
                tokenid = gs::tokenid(tokenid_str);
                CHECK_NEXT();
            }

            std::vector<std::uint64_t> token_amounts;
            token_amounts.reserve(chunks.end() - cit);
            while (cit != chunks.end()) {
                std::string amount_str = *cit;
                std::reverse(amount_str.begin(), amount_str.end());
                PARSE_CHECK(amount_str.size() != 8, "amount string size not 8 bytes");

                const std::pair<bool, std::uint64_t> value_check = string_to_number(amount_str);
                PARSE_CHECK(! value_check.first, "extraction of amount failed");
                token_amounts.push_back(value_check.second);
                ++cit;
            }

            PARSE_CHECK(token_amounts.size() == 0, "token_amounts size is 0");
            PARSE_CHECK(token_amounts.size() > 19, "token_amounts size is greater than 19");

            this->type = slp_transaction_type::send;
            this->slp_tx = slp_transaction_send(
                tokenid,
                token_amounts
            );
        } else {
            PARSE_CHECK(true, "unknown action type");
        }
    }
};

}

#endif
