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
#include <gs++/output.hpp>

namespace gs {

struct slp_output
{
    gs::outpoint  outpoint;
    std::uint64_t amount;
    bool is_mint_baton;

    slp_output(
        const gs::outpoint& outpoint,
        const std::uint64_t amount
    );

    slp_output(
        const gs::outpoint& outpoint,
        const gs::outpoint& mint_baton_utxo
    );
};

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
        const std::string&  ticker,
        const std::string&  name,
        const std::string&  document_uri,
        const std::string&  document_hash,
        const std::uint32_t decimals,
        const bool          has_mint_baton,
        const std::uint32_t mint_baton_vout,
        const std::uint64_t qty
    );
};

struct slp_transaction_mint
{
    gs::tokenid   tokenid;
    bool          has_mint_baton;
    std::uint32_t mint_baton_vout;
    std::uint64_t qty;


    slp_transaction_mint(
        const gs::tokenid&  tokenid,
        const bool          has_mint_baton, // maybe this could be function that checks if mint_baton_vout > 0
        const std::uint32_t mint_baton_vout,
        const std::uint64_t qty
    );
};

struct slp_transaction_send
{
    gs::tokenid                tokenid;
    std::vector<std::uint64_t> amounts;

    slp_transaction_send(
        const gs::tokenid&                tokenid,
        const std::vector<std::uint64_t>& amounts
    );
};

enum class slp_transaction_type {
    invalid,
    genesis,
    mint,
    send
};

struct slp_transaction
{
    slp_transaction_type type;
    absl::variant<
        slp_transaction_invalid,
        slp_transaction_genesis,
        slp_transaction_mint,
        slp_transaction_send
    > slp_tx;

    slp_transaction();
    slp_transaction(const slp_transaction_genesis& slp_tx);
    slp_transaction(const slp_transaction_mint& slp_tx);
    slp_transaction(const slp_transaction_send& slp_tx);
    slp_transaction(const gs::scriptpubkey& scriptpubkey);
};

}

#endif
