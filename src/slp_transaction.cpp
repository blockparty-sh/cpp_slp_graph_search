#include <iostream>
#include <absl/types/variant.h>
#include <gs++/slp_transaction.hpp>


namespace gs {

slp_output::slp_output(
    const gs::outpoint& outpoint,
    const std::uint64_t amount
)
: outpoint(outpoint)
, amount(amount)
, is_mint_baton(false)
{}


slp_output::slp_output(
    const gs::outpoint& outpoint,
    const gs::outpoint& mint_baton_utxo
)
: outpoint(outpoint)
, amount(0)
, is_mint_baton(true)
{}

slp_transaction_genesis::slp_transaction_genesis(
    const std::string&  ticker,
    const std::string&  name,
    const std::string&  document_uri,
    const std::string&  document_hash,
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

slp_transaction_mint::slp_transaction_mint(
    const bool          has_mint_baton, // maybe this could be function that checks if mint_baton_vout > 0
    const std::uint32_t mint_baton_vout,
    const std::uint64_t qty
)
: has_mint_baton(has_mint_baton)
, mint_baton_vout(mint_baton_vout)
, qty(qty)
{}

slp_transaction_send::slp_transaction_send(
    const std::vector<std::uint64_t>& amounts
)
: amounts(amounts)
{}

slp_transaction::slp_transaction()
: type(slp_transaction_type::invalid)
, slp_tx(slp_transaction_invalid{})
{}

slp_transaction::slp_transaction(const slp_transaction_genesis& slp_tx)
: type(slp_transaction_type::genesis)
, slp_tx(slp_tx)
{}

slp_transaction::slp_transaction(const slp_transaction_mint& slp_tx)
: type(slp_transaction_type::mint)
, slp_tx(slp_tx)
{}

slp_transaction::slp_transaction(const slp_transaction_send& slp_tx)
: type(slp_transaction_type::send)
, slp_tx(slp_tx)
{}

slp_transaction::slp_transaction(const gs::scriptpubkey& scriptpubkey)
: type(slp_transaction_type::invalid)
, slp_tx(slp_transaction_invalid{})
{
    hydrate(scriptpubkey);
}

}

std::ostream & operator<<(std::ostream &os, const gs::slp_transaction & slp)
{
    std::string slp_type = "";
    switch (slp.type) {
        case gs::slp_transaction_type::genesis: slp_type = "GENESIS"; break;
        case gs::slp_transaction_type::mint:    slp_type = "MINT";    break;
        case gs::slp_transaction_type::send:    slp_type = "SEND";    break;
        default:                                slp_type = "INVALID"; break;
    }

    os << "slp: " << slp_type << "\n";
    if (slp.type != gs::slp_transaction_type::invalid) {
        os
            << "token_type:       " << slp.token_type << "\n"
            << "tokenid:          " << slp.tokenid.decompress(true) << "\n"
            << "transaction_type: " << slp_type << "\n"
            << "\n";
    }


    if (slp.type == gs::slp_transaction_type::genesis) {
        const auto & s = absl::get<gs::slp_transaction_genesis>(slp.slp_tx);

        const auto document_hash_hex = gs::util::hex(std::vector<uint8_t>(s.document_hash.begin(), s.document_hash.end()));
        os
            << "ticker:           " << (! s.ticker.empty()          ? s.ticker          : "[none]") << "\n"
            << "name:             " << (! s.name.empty()            ? s.name            : "[none]") << "\n"
            << "document_uri:     " << (! s.document_uri.empty()    ? s.document_uri    : "[none]") << "\n"
            << "document_hash:    " << (! s.document_hash.empty()   ? document_hash_hex : "[none]") << "\n"
            << "decimals:         " << s.decimals                                                   << "\n"
            << "has_mint_baton:   " << s.has_mint_baton                                             << "\n"
            << "slp_amount:       " << (s.qty                       ? std::to_string(s.qty) : "[none]") << "\n"
            << "mint_baton_vout:  " << s.mint_baton_vout << "\n";
    }

    if (slp.type == gs::slp_transaction_type::mint) {
        const auto & s = absl::get<gs::slp_transaction_mint>(slp.slp_tx);
        os
            << "baton_vout:       " << s.mint_baton_vout          << "\n"
            << "slp_amount:       " << s.qty                      << "\n";
    }

    if (slp.type == gs::slp_transaction_type::send) {
        const auto & s = absl::get<gs::slp_transaction_send>(slp.slp_tx);

        for (auto amnt : s.amounts) {
            os  << "slp_amount:       " << amnt << "\n";
        }
    }

    return os;
}
