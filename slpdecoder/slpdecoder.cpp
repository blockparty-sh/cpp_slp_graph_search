#include <iostream>
#include <fstream>
#include <string>
#include <vector>

#define ENABLE_SLP_PARSE_ERROR_PRINTING
#include <gs++/slp_transaction.hpp>

int main(int argc, char * argv[])
{
    if (argc < 2) {
        std::cerr << "you must pass txdata" << std::endl;
        return 1;
    }

    const std::vector<std::uint8_t> slphex = gs::util::unhex(std::string(argv[1]));
    gs::slp_transaction slp;
    slp.hydrate(gs::scriptpubkey(slphex));

    std::cout << slp;

    return 0;
}
