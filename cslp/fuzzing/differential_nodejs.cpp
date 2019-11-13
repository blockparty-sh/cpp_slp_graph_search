#include <iostream>
#include <string>
#include <vector>
#include <fstream>
#include <cstdlib>
#include <iterator>

#include <nlohmann/json.hpp>
#include <boost/process.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/algorithm/hex.hpp>
#include <boost/algorithm/string/join.hpp>
#include <absl/strings/numbers.h>
#include <absl/numeric/int128.h>


#include <gs++/slp_validator.hpp>
#include <gs++/slp_transaction.hpp>

#include "util.hpp"

std::string hex(std::string in)
{
    std::string ret;
    boost::algorithm::hex(in, std::back_inserter(ret));
    return ret;
}

int main(int argc, char * argv[])
{
    if (argc < 2) {
        return 1;
    }

    std::string txdata = readfile(argv[1]);

    gs::slp_transaction slp_transaction((gs::scriptpubkey(txdata)));
    const bool hydration_success = slp_transaction.type != gs::slp_transaction_type::invalid;

    boost::process::ipstream is;
    boost::process::child c(
        "node --no-warnings nodejs_validation/run_slp_validate.js " + std::string(argv[1]),
        boost::process::std_out > is
    );

    std::vector<std::string> data;

    std::string line;
    while (c.running() && std::getline(is, line)) {
        data.push_back(line);
    }

    c.wait();
    const int exit_code = c.exit_code();

    // hairy - we look to see if true != 0 and likewise false != 1..
    ABORT_CHECK (hydration_success && !!exit_code && "c++ parsed, nodejs did not");
    ABORT_CHECK (! hydration_success && !exit_code && "c++ did not parse, but nodejs did");

    std::string joined = boost::algorithm::join(data, "\n");
    nlohmann::json j;

    try {
        j = nlohmann::json::parse(joined.begin(), joined.end());
        std::cout << j.dump(-1, ' ', true) << std::endl << std::endl;
    
    } catch (nlohmann::json::parse_error e) {
        std::cout << "ERRROR" << e.what() << std::endl;
        ABORT_CHECK (slp_transaction.type != gs::slp_transaction_type::invalid);
        return 0;
    }

    if (j["transactionType"].is_null()) {
        ABORT_CHECK (slp_transaction.type != gs::slp_transaction_type::invalid);
        return 0;
    }

    const std::string transactionType = j["transactionType"].get<std::string>();
    if (transactionType == "GENESIS") {
        ABORT_CHECK (slp_transaction.type != gs::slp_transaction_type::genesis);

        auto slp = absl::get<gs::slp_transaction_genesis>(slp_transaction.slp_tx);

        ABORT_CHECK (boost::lexical_cast<std::uint64_t>(j["versionType"]) != slp.token_type);

        // ABORT_CHECK (j["symbol"].get<std::string>() != hex(slp.ticker)); //  DISABLED FOR BAD UTF-8
        // ABORT_CHECK (j["name"].get<std::string>() != hex(slp.name));    // DISABLED FOR BAD UTF-8

        std::cout << j["documentUri"].get<std::string>() << std::endl;
        std::cout << hex(slp.document_uri) << std::endl;
        // ABORT_CHECK (j["documentUri"].get<std::string>() != hex(slp.document_uri)); // DISABLED FOR BAD UTF-8

        if (j["documentSha256"].is_null()) {
            ABORT_CHECK (slp.document_hash != "");
        } else {
            ABORT_CHECK (j["documentSha256"].get<std::string>() != hex(slp.document_hash));
        }
        ABORT_CHECK (j["decimals"].get<std::uint32_t>() != slp.decimals);
        if (j["batonVout"].is_null()) {
            ABORT_CHECK (slp.mint_baton_vout != 0);
        } else {
            ABORT_CHECK (j["batonVout"].get<std::uint32_t>() != slp.mint_baton_vout);
        }
        ABORT_CHECK (boost::lexical_cast<std::uint64_t>(j["genesisOrMintQuantity"].get<std::string>()) != slp.qty);
    }
    else if (transactionType == "MINT") {
        ABORT_CHECK (slp_transaction.type != gs::slp_transaction_type::mint);

        auto slp = absl::get<gs::slp_transaction_mint>(slp_transaction.slp_tx);

        ABORT_CHECK (boost::lexical_cast<std::uint64_t>(j["versionType"]) != slp.token_type);
        ABORT_CHECK (j["tokenIdHex"].get<std::string>() != slp.tokenid.decompress(true));
        ABORT_CHECK (boost::lexical_cast<std::uint64_t>(j["genesisOrMintQuantity"].get<std::string>()) != slp.qty);
        if (j["batonVout"].is_null()) {
            ABORT_CHECK (slp.mint_baton_vout != 0);
        } else {
            ABORT_CHECK (j["batonVout"].get<std::uint32_t>() != slp.mint_baton_vout);
        }
    }
    else if (transactionType == "SEND") {
        ABORT_CHECK (slp_transaction.type != gs::slp_transaction_type::send);

        auto slp = absl::get<gs::slp_transaction_send>(slp_transaction.slp_tx);

        ABORT_CHECK (boost::lexical_cast<std::uint64_t>(j["versionType"]) != slp.token_type);
        ABORT_CHECK (j["tokenIdHex"].get<std::string>() != slp.tokenid.decompress(true));
        std::cout << j["sendOutputs"].size() << " " << slp.amounts.size() << std::endl;
        ABORT_CHECK (j["sendOutputs"].size()-1 != slp.amounts.size());
        std::size_t sendOutputsIdx = 0;
        for (auto soutput : j["sendOutputs"]) {
            if (sendOutputsIdx > 0) {
                ABORT_CHECK (boost::lexical_cast<std::uint64_t>(soutput.get<std::string>()) != slp.amounts[sendOutputsIdx - 1]);
            }
            ++sendOutputsIdx;
        }
    } else {
        ABORT_CHECK (slp_transaction.type != gs::slp_transaction_type::invalid);
    }

    return 0;
}

