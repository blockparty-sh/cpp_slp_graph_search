#include <iostream>
#include <string>
#include <vector>
#include <fstream>
#include <cstdlib>

#include <nlohmann/json.hpp>
#include <boost/process.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/algorithm/string/join.hpp>
#include <absl/strings/numbers.h>
#include <absl/numeric/int128.h>


#include <gs++/slp_validator.hpp>
#include <gs++/transaction.hpp>

#include "util.hpp"

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
    boost::process::child c(
        "node nodejs_validation/run_slp_validate.js " + std::string(argv[1]),
        boost::process::std_out > is
    );

    std::vector<std::string> data;

    std::string line;
    while (c.running() && std::getline(is, line) && ! line.empty()) {
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
    } catch (nlohmann::json::parse_error e) {
        ABORT_CHECK (tx.slp.type != gs::slp_transaction_type::invalid);
        return 0;
    }

    if (j["transactionType"].is_null()) {
        ABORT_CHECK (tx.slp.type != gs::slp_transaction_type::invalid);
        return 0;
    }

    const std::string transactionType = j["transactionType"].get<std::string>();
    if (transactionType == "GENESIS") {
        ABORT_CHECK (tx.slp.type != gs::slp_transaction_type::genesis);

        auto slp = absl::get<gs::slp_transaction_genesis>(tx.slp.slp_tx);

        ABORT_CHECK (boost::lexical_cast<std::uint64_t>(j["versionType"]) != slp.token_type);
        ABORT_CHECK (j["symbol"].get<std::string>() != slp.ticker);
        ABORT_CHECK (j["name"].get<std::string>() != slp.name);
        ABORT_CHECK (j["documentUri"].get<std::string>() != slp.document_uri);
        ABORT_CHECK (j["documentHash"].get<std::string>() != slp.document_hash);
        ABORT_CHECK (j["decimals"].get<std::uint32_t>() != slp.decimals);
        ABORT_CHECK (j["batonVout"].get<std::uint32_t>() != slp.mint_baton_vout);
        ABORT_CHECK (boost::lexical_cast<std::uint64_t>(j["genesisOrMintQuantity"].get<std::string>()) != slp.qty);
    }
    else if (transactionType == "MINT") {
        ABORT_CHECK (tx.slp.type != gs::slp_transaction_type::mint);

        auto slp = absl::get<gs::slp_transaction_mint>(tx.slp.slp_tx);

        ABORT_CHECK (boost::lexical_cast<std::uint64_t>(j["versionType"]) != slp.token_type);
        ABORT_CHECK (j["tokenIdHex"].get<std::string>() != slp.tokenid.decompress(true));
        ABORT_CHECK (boost::lexical_cast<std::uint64_t>(j["genesisOrMintQuantity"].get<std::string>()) != slp.qty);
        ABORT_CHECK (j["batonVout"].get<std::uint32_t>() != slp.mint_baton_vout);
    }
    else if (transactionType == "SEND") {
        ABORT_CHECK (tx.slp.type != gs::slp_transaction_type::send);

        auto slp = absl::get<gs::slp_transaction_send>(tx.slp.slp_tx);

        ABORT_CHECK (boost::lexical_cast<std::uint64_t>(j["versionType"]) != slp.token_type);
        ABORT_CHECK (j["tokenIdHex"].get<std::string>() != slp.tokenid.decompress(true));
        ABORT_CHECK (j["sendOutputs"].size() != slp.amounts.size());
        std::size_t sendOutputsIdx = 0;
        for (auto soutput : j["sendOutputs"]) {
            ABORT_CHECK (boost::lexical_cast<std::uint64_t>(soutput.get<std::string>()) != slp.amounts[sendOutputsIdx]);
            ++sendOutputsIdx;
        }
    } else {
        ABORT_CHECK (tx.slp.type != gs::slp_transaction_type::invalid);
    }

    return 0;
}

