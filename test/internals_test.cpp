#define CATCH_CONFIG_MAIN

#define ENABLE_SLP_PARSE_ERROR_PRINTING

#include <string>
#include <fstream>
#include <streambuf>

#include <nlohmann/json.hpp>
#include <catch2/catch.hpp>
#include <gs++/txgraph.hpp>
#include <gs++/utxodb.hpp>
#include <gs++/transaction.hpp>
#include <gs++/scriptpubkey.hpp>
#include <gs++/output.hpp>
#include <gs++/util.hpp>


int create_txgraph()
{
    gs::txgraph g;
    return 1;
}

TEST_CASE( "create_txgraph is 1 (pass)", "[single-file]" ) {
    REQUIRE( create_txgraph() == 1 );
}


TEST_CASE( "check_slpdb_validity", "[single-file]" ) {
	std::ifstream test_data_stream("./test/script_tests.json");
	std::string test_data_str((std::istreambuf_iterator<char>(test_data_stream)),
							   std::istreambuf_iterator<char>());

	auto test_data = nlohmann::json::parse(test_data_str);

    gs::utxodb utxodb;
    for (auto m : test_data) {
        //std::cout << m["msg"] << "\n";
        SECTION(m["msg"]) {
            const bool valid = m["code"].is_null();
            //const int code = m.get<int>("code");

            const std::string script_str = m["script"].get<std::string>();
            std::cout << script_str << std::endl;
            const std::vector<std::uint8_t> script = gs::util::compress_hex(script_str);

            gs::output output({}, 0, 0, 0, gs::scriptpubkey(script));
            REQUIRE( output.is_valid_slp() == valid );
        }
    }
}
