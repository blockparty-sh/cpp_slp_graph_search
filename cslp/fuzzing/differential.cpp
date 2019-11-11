#include <iostream>
#include <string>
#include <vector>
#include <fstream>
#include <cstdlib>

#include <nlohmann/json.hpp>
#include <boost/process.hpp>
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
	boost::process::child c("node nodejs_validation/run_slp_validate.js "+std::string(argv[1]),
		boost::process::std_out > is);

	std::vector<std::string> data;

	std::string line;
	while (c.running() && std::getline(is, line) && ! line.empty()) {
		data.push_back(line);
	}

	c.wait();
	int result = c.exit_code();

	// hairy - we look to see if true != 0 and likewise false != 1..
	if (! hydration_success != !!result) {
		abort();
	}

	std::string joined = boost::algorithm::join(data, "\n");
	nlohmann::json j;
    
    try {
        j = nlohmann::json::parse(joined.begin(), joined.end());
    } catch (nlohmann::json::parse_error e) {
        if (tx.slp.type != gs::slp_transaction_type::invalid) {
            abort();
        }

        return 0;
    }


	if (j["transactionType"].is_null() && tx.slp.type != gs::slp_transaction_type::invalid) {
		abort();
	}

    if (j["transactionType"].is_null()) {
        return 0;
    }

    const std::string transactionType = j["transactionType"].get<std::string>();
    if (transactionType == "GENESIS") {
        if (tx.slp.type != gs::slp_transaction_type::genesis) {
            abort();
        }

        auto slp = absl::get<gs::slp_transaction_genesis>(tx.slp.slp_tx);

        const std::string symbol = j["symbol"].get<std::string>();
        if (symbol != slp.ticker) {
            abort();
        }

        const std::string name = j["name"].get<std::string>();
        if (name != slp.name) {
            abort();
        }

        const std::string document_uri = j["documentUri"].get<std::string>();
        if (document_uri != slp.document_uri) {
            abort();
        }

        const std::string document_hash = j["documentHash"].get<std::string>();
        if (document_hash != slp.document_hash) {
            abort();
        }

        const std::uint32_t decimals = j["decimals"].get<std::uint32_t>();
        if (decimals != slp.decimals) {
            abort();
        }

        const std::uint32_t mint_baton_vout = j["batonVout"].get<std::uint32_t>();
        if (mint_baton_vout != slp.mint_baton_vout) {
            abort();
        }

        /*
         * TODO
        std::string genesisOrMintQuantityStr = j["genesisOrMintQuantity"].get<std::string>();
        const absl::uint128 genesisOrMintQuantity = 0;
        absl::SimpleAtoi(genesisOrMintQuantityStr, genesisOrMintQuantity);
        if (genesisOrMintQuantity != slp.qty) {
            abort();
        }
        */
    }

    if (transactionType == "MINT") {
        if (tx.slp.type != gs::slp_transaction_type::mint) {
            abort();
        }

        auto slp = absl::get<gs::slp_transaction_mint>(tx.slp.slp_tx);
        // TODO check other mint attributes
    }

    if (transactionType == "SEND") {
        if (tx.slp.type != gs::slp_transaction_type::send) {
            abort();
        }

        auto slp = absl::get<gs::slp_transaction_mint>(tx.slp.slp_tx);

        if (j["tokenIdHex"].get<std::string>() != slp.tokenid.decompress(true)) {
            abort();
        }

        for (auto soutput : j["sendOutputs"]) {
            // TODO compare sendoutputs
        }
    }

    return 0;
}

