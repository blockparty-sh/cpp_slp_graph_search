#include <iostream>
#include <fstream>
#include <string>
#include <vector>

#include <gs++/transaction.hpp>

int main(int argc, char * argv[])
{
    if (argc < 2) {
        std::cerr << "you must pass txdata" << std::endl;
        return 1;
    }

    const std::vector<std::uint8_t> txhex = gs::util::compress_hex(std::string(argv[1]));
    gs::transaction tx;
    if (! tx.hydrate(txhex.begin(), txhex.end(), 0) ) {
        std::cerr << "tx hydration failed" << std::endl;
        return 1;
    }

    std::cout << tx << std::endl;

    return 0;
}
