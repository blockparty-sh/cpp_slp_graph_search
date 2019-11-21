#include <iostream>
#include <fstream>
#include <string>
#include <vector>

#include "util.hpp"
#include <gs++/slp_transaction.hpp>

int main(int argc, char * argv[])
{
    if (argc < 2) {
        return 1;
    }

    std::string txdata = readfile(argv[1]);
    gs::slp_transaction tx((gs::scriptpubkey(txdata)));

    return 0;
}
