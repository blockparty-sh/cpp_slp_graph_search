#include <iostream>
#include <fstream>
#include <string>
#include <vector>

#define ENABLE_BCH_PARSE_PRINTING
#include <gs++/transaction.hpp>

int main(int argc, char * argv[])
{
    if (argc < 2) {
        std::cerr << "you must pass txdata" << std::endl;
        return 1;
    }

    const std::vector<std::uint8_t> txhex = gs::util::unhex(std::string(argv[1]));
    gs::transaction tx;
    if (! tx.hydrate(txhex.begin(), txhex.end()) ) {
        std::cerr << "tx hydration failed" << std::endl;
        return 1;
    }

    std::cout << tx;

    return 0;
}
