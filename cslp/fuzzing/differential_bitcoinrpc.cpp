#include <iostream>
#include <string>
#include <vector>
#include <fstream>
#include <cstdlib>

#include <nlohmann/json.hpp>
#include <boost/process.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/algorithm/hex.hpp>
#include <boost/algorithm/string/join.hpp>
#include <absl/strings/numbers.h>
#include <absl/numeric/int128.h>


#include <gs++/slp_validator.hpp>
#include <gs++/transaction.hpp>


std::string readfile(const std::string &fileName)
{
    std::ifstream ifs(fileName.c_str(), std::ios::in | std::ios::binary | std::ios::ate);

    std::ifstream::pos_type fileSize = ifs.tellg();
    ifs.seekg(0, std::ios::beg);

    std::vector<char> bytes(fileSize);
    ifs.read(bytes.data(), fileSize);

    return std::string(bytes.data(), fileSize);
}

int main(int argc, char * argv[])
{
    if (argc < 2) {
        return 1;
    }

    std::string txdata = readfile(argv[1]);

    gs::slp_validator slp_validator;
    gs::transaction tx;
    const bool hydration_success = tx.hydrate(txdata.begin(), txdata.end(), 0);

    boost::process::ipstream is;
    std::string hex_str;
    boost::algorithm::hex(txdata, std::back_inserter(hex_str));
    boost::process::child c(
        "bitcoin-cli decoderawtransaction \"" + hex_str + "\"",
        boost::process::std_out > is
    );

    std::vector<std::string> data;

    std::string line;
    while (c.running() && std::getline(is, line) && ! line.empty()) {
        data.push_back(line);
    }

    c.wait();
    const int exit_code = c.exit_code();


#define ABORT_CHECK(cond) {\
    if ((cond)) { \
        std::cerr << #cond << std::endl;\
        abort();\
    }\
}

    // hairy - we look to see if true != 0 and likewise false != 1..
    ABORT_CHECK (hydration_success && !!exit_code && "c++ parsed, nodejs did not");
    ABORT_CHECK (! hydration_success && !exit_code && "c++ did not parse, but nodejs did");

    // TODO we should check json output here and compare to the gs::transaction

    return 0;
}

