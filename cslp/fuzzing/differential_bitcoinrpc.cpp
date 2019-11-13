#include <iostream>
#include <string>
#include <vector>
#include <fstream>
#include <cstdlib>

#include <nlohmann/json.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/algorithm/hex.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/join.hpp>
#include <boost/format.hpp>
#include <absl/strings/numbers.h>
#include <absl/numeric/int128.h>

#include <gs++/rpc.hpp>
#include <gs++/slp_validator.hpp>
#include <gs++/transaction.hpp>

#include "util.hpp"
#include "rpc_config.hpp"

std::uint64_t bch2sats (long double v)
{
    std::string bch = boost::str(boost::format("%.8f") % v);

    std::uint64_t ret = 0;

    std::vector<std::string> parts;
    boost::split(parts, bch, boost::is_any_of("."));

    if (parts.size() == 0) {
        return ret;
    }

    ret += boost::lexical_cast<std::uint64_t>(parts[0]) * 100000000;

    if (parts.size() == 1) {
        return ret;
    }

    boost::algorithm::trim_left_if(parts[1], [](char c) { return c == '0'; });

    if (parts[1].empty()) {
        return ret;
    }

    ret += boost::lexical_cast<std::uint64_t>(parts[1]);
    return ret;
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

    std::string hex_str;
    boost::algorithm::hex(txdata, std::back_inserter(hex_str));

    gs::rpc rpc(RPC_HOST, RPC_PORT, RPC_USER, RPC_PASS);
    std::pair<bool, nlohmann::json> decoded_tx;
    
    try {
        decoded_tx = rpc.get_decode_raw_transaction(hex_str);
    } catch (nlohmann::json::parse_error e) {
        std::cout << e.what() << std::endl;
        // we have weaker guarantees on tx validity than bitcoin
        // ABORT_CHECK (hydration_success);
        return 0;
    }

    if (! hydration_success && ! decoded_tx.first) {
        return 0;
    }

    // hairy - we look to see if true != 0 and likewise false != 1..
    if (hydration_success && ! decoded_tx.first) {
        return 0;
    }

    // we have weaker guarantees on tx validity than bitcoin
    // ABORT_CHECK (hydration_success && ! decoded_tx.first && "c++ parsed, bitcoin did not");
    ABORT_CHECK (! hydration_success && decoded_tx.first && "c++ did not parse, but bitcoin did");

    /*
    std::cout
        << "hydration_success: " << hydration_success << "\n"
        << "decoded_tx.first: " << decoded_tx.first << "\n"
        << "decoded_tx.second: " << decoded_tx.second << "\n";
    */

    nlohmann::json j = decoded_tx.second;
    std::cout << j << std::endl;

    // std::cout << j["txid"].get<std::string>() << std::endl;
    std::cout << tx.txid.decompress(true) << std::endl;

    // std::cout << std::endl;

    // std::cout << j["version"].get<std::int32_t>() << std::endl;
    std::cout << tx.version << std::endl;

    // std::cout << std::endl;


    ABORT_CHECK (j["txid"].get<std::string>() != tx.txid.decompress(true));
    ABORT_CHECK (j["hash"].get<std::string>() != tx.txid.decompress(true));
    ABORT_CHECK (j["version"].get<std::int32_t>() != tx.version);

    ABORT_CHECK (j["vin"].size() != tx.inputs.size());
    std::size_t inputsIdx = 0;
    for (auto v : j["vin"]) {
        ABORT_CHECK (v["txid"].get<std::string>() != tx.inputs[inputsIdx].txid.decompress(true));
        ABORT_CHECK (v["vout"].get<std::uint32_t>() != tx.inputs[inputsIdx].vout);
        ++inputsIdx;
    }

    ABORT_CHECK (j["vout"].size() != tx.outputs.size());
    for (auto v : j["vout"]) {
        const std::size_t outputsIdx = v["n"].get<std::uint64_t>();

        const std::uint64_t sats = bch2sats(v["value"].get<long double>()); 
        // otherwise its non-sensical
        if (sats < 21000000ull * 100000000ull) {
            std::cout << sats << std::endl;
            std::cout << tx.outputs[outputsIdx].value << std::endl;
            ABORT_CHECK (sats != tx.outputs[outputsIdx].value);
        }

        ABORT_CHECK (v["scriptPubKey"]["hex"].get<std::string>()
            != gs::util::decompress_hex(tx.outputs[outputsIdx].scriptpubkey.v));

    }

    // std::cout << j["locktime"].get<std::uint32_t>() << std::endl;
    std::cout << tx.lock_time << std::endl;
    ABORT_CHECK (j["locktime"].get<std::uint32_t>() != tx.lock_time);

    return 0;
}

