#include <iostream>
#include <fstream>
#include <string>
#include <vector>

#include "util.hpp"
#include <gs++/transaction.hpp>

int main(int argc, char * argv[])
{
    if (argc < 2) {
        return 1;
    }

    std::string txdata = readfile(argv[1]);

    gs::transaction tx;
    const bool hydration_success = tx.hydrate(txdata.begin(), txdata.end(), 0);

    return 0;
}
