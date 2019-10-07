#define CATCH_CONFIG_MAIN

// #define ENABLE_SLP_PARSE_ERROR_PRINTING

#include <string>
#include <fstream>
#include <streambuf>

#include <nlohmann/json.hpp>
#include <catch2/catch.hpp>
#include <gs++/txgraph.hpp>
#include <gs++/scriptpubkey.hpp>
#include <gs++/util.hpp>
#include <gs++/slp_transaction.hpp>


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
        SECTION(m["msg"]) {
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
        SECTION(m["msg"]) {
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
                REQUIRE( slp.tokenid        == m["data"]["tokenid"].get<std::string>() );
                REQUIRE( slp.has_mint_baton == m["data"]["has_mint_baton"].get<bool>() );

                if (m["data"]["has_mint_baton"].get<bool>()) {
                    REQUIRE( slp.mint_baton_vout == m["data"]["mint_baton_vout"].get<std::uint32_t>() );
                }

                REQUIRE( slp.qty == m["data"]["additional_qty"].get<std::uint64_t>() );
            }
            else if (test_tx_type == "SEND") {
                REQUIRE( tx.type == gs::slp_transaction_type::send );

                const auto slp = std::get<gs::slp_transaction_send>(tx.slp_tx);
                REQUIRE( slp.tokenid == m["data"]["tokenid"].get<std::string>() );

                std::vector<std::uint64_t> amounts;
                for (auto amount_json : m["data"]["amounts"]) {
                    amounts.push_back(amount_json.get<std::uint64_t>());
                }

                REQUIRE( slp.amounts == amounts );
            }
        }
    }
}
