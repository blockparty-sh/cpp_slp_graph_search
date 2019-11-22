#include <iostream>
#include <fstream>
#include <string>
#include <vector>

#include <gs++/block.hpp>

int main(int argc, char * argv[])
{
    if (argc < 2) {
        std::cerr << "you must pass blockdata" << std::endl;
        return 1;
    }

    const std::vector<std::uint8_t> blockhex = gs::util::compress_hex(std::string(argv[1]));
    gs::block block;
    if (! block.hydrate(blockhex.begin(), blockhex.end()) ) {
        std::cerr << "block hydration failed" << std::endl;
        return 1;
    }

    std::cout << block << std::endl;

    return 0;
}
