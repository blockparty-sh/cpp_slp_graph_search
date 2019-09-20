#define CATCH_CONFIG_MAIN

#include <catch2/catch.hpp>
#include <gs++/txgraph.hpp>


int create_txgraph()
{
    txgraph g;
    return 1;
}

TEST_CASE( "create_txgraph is 1 (pass)", "[single-file]" ) {
    REQUIRE( create_txgraph() == 1 );
}
