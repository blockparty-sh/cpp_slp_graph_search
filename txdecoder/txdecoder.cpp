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

    std::cout
        << "txid:      " << tx.txid.decompress(true) << "\n"
        << "version:   " << tx.version << "\n"
        << "lock_time: " << tx.lock_time << "\n\n";

    std::cout << "inputs:\n";
    for (auto m : tx.inputs) {
        std::cout
            << "\ttxid: " << m.txid.decompress(true) << "\n"
            << "\tvout: " << m.vout << "\n\n";
    }

    std::cout << "outputs:\n";
    for (auto m : tx.outputs) {
        std::cout
            << "\tvalue:        " << m.value << "\n"
            << "\tscriptpubkey: " << gs::util::decompress_hex(m.scriptpubkey.v) << "\n\n";
    }


    std::string slp_type = "";
    switch (tx.slp.type) {
        case gs::slp_transaction_type::genesis: slp_type = "GENESIS"; break;
        case gs::slp_transaction_type::mint:    slp_type = "MINT";    break;
        case gs::slp_transaction_type::send:    slp_type = "SEND";    break;
        default:                                slp_type = "INVALID"; break;
    }

    std::cout
        << "slp:\n"
        << "\ttoken_type:       " << tx.slp.token_type << "\n" 
        << "\ttransaction_type: " << slp_type << "\n";

    if (tx.slp.type == gs::slp_transaction_type::genesis) {
        const auto & slp = absl::get<gs::slp_transaction_genesis>(tx.slp.slp_tx);
        std::cout
            << "\tticker:           " << slp.ticker          << "\n"
            << "\tname:             " << slp.name            << "\n"
            << "\tdocument_uri:     " << slp.document_uri    << "\n"
            << "\tdocument_hash:    " << slp.document_hash   << "\n"
            << "\tdecimals:         " << slp.decimals        << "\n"
            << "\thas_mint_baton:   " << slp.has_mint_baton  << "\n"
            << "\tmint_baton_vout:  " << slp.mint_baton_vout << "\n"
            << "\tqty:              " << slp.qty             << "\n";
    }
    if (tx.slp.type == gs::slp_transaction_type::mint) {
        const auto & slp = absl::get<gs::slp_transaction_mint>(tx.slp.slp_tx);
        std::cout
            << "\ttokenid:          " << slp.tokenid.decompress(true) << "\n"
            << "\tmint_baton_vout:  " << slp.mint_baton_vout          << "\n"
            << "\tqty:              " << slp.qty                      << "\n";
    }
    if (tx.slp.type == gs::slp_transaction_type::send) {
        const auto & slp = absl::get<gs::slp_transaction_send>(tx.slp.slp_tx);
        std::cout
            << "\ttokenid:          " << slp.tokenid.decompress(true) << "\n";

        std::cout << "\tamounts:\n";
        for (auto m : slp.amounts) {
            std::cout << "\t\t" << m << "\n";
        }
    }

    return 0;
}
