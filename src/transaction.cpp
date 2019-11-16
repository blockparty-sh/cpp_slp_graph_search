#include <gs++/transaction.hpp>

std::ostream & operator<<(std::ostream &os, const gs::transaction & tx)
{
    os
        << "txid:      " << tx.txid.decompress(true) << "\n"
        << "version:   " << tx.version << "\n"
        << "lock_time: " << tx.lock_time << "\n\n";

    os << "inputs:\n";
    for (auto m : tx.inputs) {
        os
            << "\ttxid: " << m.txid.decompress(true) << "\n"
            << "\tvout: " << m.vout << "\n\n";
    }

    os << "outputs:\n";
    std::size_t n = 0;
    for (auto m : tx.outputs) {
        os
            << "\tn:            " << n                                          << "\n"
            << "\tvalue:        " << m.value                                    << "\n"
            << "\tscriptpubkey: " << gs::util::decompress_hex(m.scriptpubkey.v) << "\n\n";
        ++n;
    }


    std::string slp_type = "";
    switch (tx.slp.type) {
        case gs::slp_transaction_type::genesis: slp_type = "GENESIS"; break;
        case gs::slp_transaction_type::mint:    slp_type = "MINT";    break;
        case gs::slp_transaction_type::send:    slp_type = "SEND";    break;
        default:                                slp_type = "INVALID"; break;
    }

    os
        << "slp:\n"
        << "\ttoken_type:       " << tx.slp.token_type << "\n"
        << "\ttransaction_type: " << slp_type << "\n";

    if (tx.slp.type == gs::slp_transaction_type::genesis) {
        const auto & slp = absl::get<gs::slp_transaction_genesis>(tx.slp.slp_tx);
        os
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
        os
            << "\ttokenid:          " << slp.tokenid.decompress(true) << "\n"
            << "\tmint_baton_vout:  " << slp.mint_baton_vout          << "\n"
            << "\tqty:              " << slp.qty                      << "\n";
    }
    if (tx.slp.type == gs::slp_transaction_type::send) {
        const auto & slp = absl::get<gs::slp_transaction_send>(tx.slp.slp_tx);
        os
            << "\ttokenid:          " << slp.tokenid.decompress(true) << "\n";

        os << "\tamounts:\n";
        for (auto m : slp.amounts) {
            os << "\t\t" << m << "\n";
        }
    }

    return os;
}

