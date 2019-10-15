#define CATCH_CONFIG_MAIN

// #define ENABLE_SLP_PARSE_ERROR_PRINTING

#include <string>
#include <fstream>
#include <streambuf>
#include <algorithm>
#include <vector>
#include <string>

#include <nlohmann/json.hpp>
#include <catch2/catch.hpp>
#include <gs++/txgraph.hpp>
#include <gs++/scriptpubkey.hpp>
#include <gs++/util.hpp>
#include <gs++/slpdb.hpp>
#include <gs++/slp_transaction.hpp>
#include <gs++/bch.hpp>


int create_txgraph()
{
    gs::txgraph g;
    return 1;
}

TEST_CASE( "create_txgraph is 1 (pass)", "[single-file]" ) {
    REQUIRE( create_txgraph() == 1 );
}


TEST_CASE( "script_tests", "[single-file]" ) {
	std::ifstream test_data_stream("./test/script_tests.json");
	std::string test_data_str((std::istreambuf_iterator<char>(test_data_stream)),
							   std::istreambuf_iterator<char>());

	auto test_data = nlohmann::json::parse(test_data_str);

    for (auto m : test_data) {
        SECTION(m["msg"].get<std::string>()) {
            const bool valid = m["code"].is_null();
            const std::string script_str = m["script"].get<std::string>();

            const std::vector<std::uint8_t> script = gs::util::compress_hex(script_str);

            gs::slp_transaction tx = gs::slp_transaction(gs::scriptpubkey(script));

            if (valid) REQUIRE( tx.type != gs::slp_transaction_type::invalid );
            else       REQUIRE( tx.type == gs::slp_transaction_type::invalid );
        }
    }
}

TEST_CASE( "slp_decoding_tx_tests", "[single-file]" ) {
	std::ifstream test_data_stream("./test/slp_decoding_tx_tests.json");
	std::string test_data_str((std::istreambuf_iterator<char>(test_data_stream)),
							   std::istreambuf_iterator<char>());

	auto test_data = nlohmann::json::parse(test_data_str);

    for (auto m : test_data) {
        SECTION(m["msg"].get<std::string>()) {
            const std::string script_str = m["script"].get<std::string>();
            const std::string test_tx_type = m["type"].get<std::string>();

            const std::vector<std::uint8_t> script = gs::util::compress_hex(script_str);

            gs::slp_transaction tx = gs::slp_transaction(gs::scriptpubkey(script));

            if (test_tx_type == "INVALID") {
                REQUIRE( tx.type == gs::slp_transaction_type::invalid );
            }
            else if (test_tx_type == "GENESIS") {
                REQUIRE( tx.type == gs::slp_transaction_type::genesis );

                const auto slp = std::get<gs::slp_transaction_genesis>(tx.slp_tx);
                REQUIRE( slp.ticker          == m["data"]["ticker"].get<std::string>() );
                REQUIRE( slp.name            == m["data"]["name"].get<std::string>() );
                REQUIRE( slp.document_uri    == m["data"]["document_uri"].get<std::string>() );
                REQUIRE( slp.document_hash   == m["data"]["document_hash"].get<std::string>() );
                REQUIRE( slp.decimals        == m["data"]["decimals"].get<std::uint32_t>() );
                REQUIRE( slp.has_mint_baton  == m["data"]["has_mint_baton"].get<bool>() );

                if (m["data"]["has_mint_baton"].get<bool>()) {
                    REQUIRE( slp.mint_baton_vout == m["data"]["mint_baton_vout"].get<std::uint32_t>() );
                }

                REQUIRE( slp.qty == m["data"]["initial_qty"].get<std::uint64_t>() );
            }
            else if (test_tx_type == "MINT") {
                REQUIRE( tx.type == gs::slp_transaction_type::mint );

                const auto slp = std::get<gs::slp_transaction_mint>(tx.slp_tx);

                std::vector<std::uint8_t> tokenid_bytes = gs::util::compress_hex(m["data"]["tokenid"].get<std::string>());
                std::reverse(tokenid_bytes.begin(), tokenid_bytes.end());
                gs::tokenid  tokenid(tokenid_bytes);

                REQUIRE( slp.tokenid        == tokenid );
                REQUIRE( slp.has_mint_baton == m["data"]["has_mint_baton"].get<bool>() );

                if (m["data"]["has_mint_baton"].get<bool>()) {
                    REQUIRE( slp.mint_baton_vout == m["data"]["mint_baton_vout"].get<std::uint32_t>() );
                }

                REQUIRE( slp.qty == m["data"]["additional_qty"].get<std::uint64_t>() );
            }
            else if (test_tx_type == "SEND") {
                REQUIRE( tx.type == gs::slp_transaction_type::send );

                const auto slp = std::get<gs::slp_transaction_send>(tx.slp_tx);

                std::vector<std::uint8_t> tokenid_bytes = gs::util::compress_hex(m["data"]["tokenid"].get<std::string>());
                std::reverse(tokenid_bytes.begin(), tokenid_bytes.end());
                gs::tokenid  tokenid(tokenid_bytes);

                REQUIRE( slp.tokenid == tokenid );

                std::vector<std::uint64_t> amounts;
                for (auto amount_json : m["data"]["amounts"]) {
                    amounts.push_back(amount_json.get<std::uint64_t>());
                }

                REQUIRE( slp.amounts == amounts );
            }
        }
    }
}

TEST_CASE( "bch_decoding_tx_to_slp_tests", "[single-file]" ) {
	std::ifstream test_data_stream("./test/bch_decoding_tx_to_slp_tests.json");
	std::string test_data_str((std::istreambuf_iterator<char>(test_data_stream)),
							   std::istreambuf_iterator<char>());

	auto test_data = nlohmann::json::parse(test_data_str);

    for (auto m : test_data) {
        SECTION(m["msg"].get<std::string>()) {
            gs::slpdb slpdb;

            for (auto& j_tx : m["transactions"]) {
                const std::vector<std::uint8_t> txhex = gs::util::compress_hex(j_tx.get<std::string>());
                slpdb.add_transaction(gs::transaction(txhex.begin(), 0));
            }

            for (auto rit : m["result"].items()) {
                std::vector<std::uint8_t> tokenid_bytes = gs::util::compress_hex(rit.key());
                std::reverse(tokenid_bytes.begin(), tokenid_bytes.end());
                gs::tokenid  tokenid(tokenid_bytes);
                auto token_search = slpdb.tokens.find(tokenid);

                REQUIRE( token_search != slpdb.tokens.end() );
                if (token_search == slpdb.tokens.end()) {
                    continue;
                }

                gs::slp_token & token = token_search->second;

                if (rit.value()["mint_baton_outpoint"].is_null()) {
                    REQUIRE( ! token.mint_baton_outpoint.has_value() );
                } else {
                    std::vector<std::uint8_t> txid_bytes = gs::util::compress_hex(
                        rit.value()["mint_baton_outpoint"]["txid"].get<std::string>()
                    );
                    std::reverse(txid_bytes.begin(), txid_bytes.end());
                    const gs::outpoint outpoint(
                        gs::txid(txid_bytes),
                        rit.value()["mint_baton_outpoint"]["vout"].get<std::uint32_t>()
                    );

                    REQUIRE( token.mint_baton_outpoint.has_value() );

                    if (token.mint_baton_outpoint.has_value()) {
                        REQUIRE( token.mint_baton_outpoint.value().txid == outpoint.txid );
                        REQUIRE( token.mint_baton_outpoint.value().vout == outpoint.vout );
                    }
                }

                for (auto utxo : rit.value()["utxos"]) {
                    std::vector<std::uint8_t> txid_bytes = gs::util::compress_hex(
                        utxo["txid"].get<std::string>()
                    );
                    std::reverse(txid_bytes.begin(), txid_bytes.end());
                    const gs::outpoint outpoint(
                        gs::txid(txid_bytes),
                        utxo["vout"].get<std::uint32_t>()
                    );

                    auto utxo_search = token.utxos.find(outpoint);
                    REQUIRE( utxo_search != token.utxos.end() );
                    if (utxo_search == token.utxos.end()) {
                        continue;
                    }

                    gs::slp_output& op = utxo_search->second;

                    REQUIRE( outpoint.txid == op.outpoint.txid );
                    REQUIRE( outpoint.vout == op.outpoint.vout );
                    REQUIRE( utxo["amount"].get<std::uint64_t>() == op.amount );
                }
            }
        }
    }
}

TEST_CASE( "topological_sorting", "[single-file]" ) {
	std::ifstream test_data_stream("./test/topological_sorting.json");
	std::string test_data_str((std::istreambuf_iterator<char>(test_data_stream)),
							   std::istreambuf_iterator<char>());

	auto test_data = nlohmann::json::parse(test_data_str);

    for (auto m : test_data) {
        SECTION(m["msg"].get<std::string>()) {
            gs::bch bch;

            std::vector<std::string> txdata_strs;
            for (auto& j_tx : m["transactions"]) {
                txdata_strs.push_back(j_tx.get<std::string>());
            }

            std::vector<gs::txid> ordered;
            for (auto j_txid : m["result"]) {
                std::vector<std::uint8_t> txid_bytes = gs::util::compress_hex(j_txid.get<std::string>());
                std::reverse(txid_bytes.begin(), txid_bytes.end());
                gs::txid txid(txid_bytes);
                ordered.push_back(txid);
            }

            // we need to sort for next_permutation to work correctly
            std::sort(txdata_strs.begin(), txdata_strs.end());

            std::size_t permutation_idx=0;
            do {
                SECTION ("\tpermutation: "+std::to_string(permutation_idx)) {
                    std::vector<gs::transaction> transactions;
                    for (const std::string & tx_str : txdata_strs) {
                        const std::vector<std::uint8_t> txhex = gs::util::compress_hex(tx_str);
                        transactions.push_back(gs::transaction(txhex.begin(), 0));
                    }

                    std::vector<gs::transaction> sorted_txs = bch.topological_sort(transactions);
                    for (std::size_t i=0; i<sorted_txs.size(); ++i) {
                        REQUIRE( sorted_txs[i].txid.decompress(true) == ordered[i].decompress(true) );
                    }
                }
                ++permutation_idx;
            } while (std::next_permutation(txdata_strs.begin(), txdata_strs.end()));

        }
    }
}
