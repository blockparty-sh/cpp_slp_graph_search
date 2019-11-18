#include <gs++/transaction.hpp>

namespace gs {

std::uint64_t transaction::output_slp_amount(const std::uint64_t vout) const
{
    if      (slp.type == slp_transaction_type::send) {
        const auto & s = absl::get<gs::slp_transaction_send>(slp.slp_tx);

        if (vout > 0 && vout-1 < s.amounts.size()) {
            return s.amounts[vout-1];
        }
    }
    else if (slp.type == slp_transaction_type::mint) {
        const auto & s = absl::get<gs::slp_transaction_mint>(slp.slp_tx);
        if (vout == 1) {
            return s.qty;
        }
    }
    else if (slp.type == slp_transaction_type::genesis) {
        const auto & s = absl::get<gs::slp_transaction_genesis>(slp.slp_tx);
        if (vout == 1) {
            return s.qty;
        }
    }

    return 0;
}

gs::outpoint transaction::mint_baton_outpoint() const
{
    if (slp.type == slp_transaction_type::mint) {
        const auto & s = absl::get<gs::slp_transaction_mint>(slp.slp_tx);
        if (s.mint_baton_vout < outputs.size()) {
            return gs::outpoint(txid, s.mint_baton_vout);
        }
    }
    else if (slp.type == slp_transaction_type::genesis) {
        const auto & s = absl::get<gs::slp_transaction_genesis>(slp.slp_tx);
        if (s.mint_baton_vout < outputs.size()) {
            return gs::outpoint(txid, s.mint_baton_vout);
        }
    }

    return gs::outpoint(txid, 0);
}

}

std::ostream & operator<<(std::ostream &os, const gs::transaction & tx)
{
    os
        << "txid:             " << tx.txid.decompress(true) << "\n"
        << "version:          " << tx.version << "\n"
        << "lock_time:        " << tx.lock_time << "\n";


    std::string slp_type = "";
    switch (tx.slp.type) {
        case gs::slp_transaction_type::genesis: slp_type = "GENESIS"; break;
        case gs::slp_transaction_type::mint:    slp_type = "MINT";    break;
        case gs::slp_transaction_type::send:    slp_type = "SEND";    break;
        default:                                slp_type = "INVALID"; break;
    }

    if (tx.slp.type == gs::slp_transaction_type::invalid) {
        os
            << "slp: INVALID\n";
    } else {
        os
            << "token_type:       " << tx.slp.token_type << "\n"
            << "tokenid:          " << tx.slp.tokenid.decompress(true) << "\n"
            << "transaction_type: " << slp_type << "\n";
    }
    os
        << "\n";

    if (tx.slp.type == gs::slp_transaction_type::genesis) {
        const auto & slp = absl::get<gs::slp_transaction_genesis>(tx.slp.slp_tx);

        os
            << "ticker:           " << (! slp.ticker.empty()          ? slp.ticker          : "[none]") << "\n"
            << "name:             " << (! slp.name.empty()            ? slp.name            : "[none]") << "\n"
            << "document_uri:     " << (! slp.document_uri.empty()    ? slp.document_uri    : "[none]") << "\n"
            << "document_hash:    " << (! slp.document_hash.empty()   ? slp.document_hash   : "[none]") << "\n"
            << "decimals:         " << slp.decimals                                                     << "\n"
            << "has_mint_baton:   " << slp.has_mint_baton                                               << "\n";
        if (slp.has_mint_baton) {
            os
                << "mint_baton_vout:  " << slp.mint_baton_vout << "\n";
        }
        os
            << "\n";

    }

    os << "inputs:\n";
    std::size_t in = 0;
    for (auto m : tx.inputs) {
        os
            << "#" << in << "\n"
            << "    txid: " << m.txid.decompress(true) << "\n"
            << "    vout: " << m.vout << "\n\n";
        ++in;
    }

    os << "outputs:\n";
    std::size_t on = 0;
    for (auto m : tx.outputs) {
        os
            << "#" << on << "\n"
            << "    value:        " << m.value                                    << "\n"
            << "    scriptpubkey: " << gs::util::decompress_hex(m.scriptpubkey.v) << "\n";

        if (tx.slp.type == gs::slp_transaction_type::genesis) {
            const auto & slp = absl::get<gs::slp_transaction_genesis>(tx.slp.slp_tx);
            if (on == 1) {
                os
                    << "    slp_amount:   " << (slp.qty ? std::to_string(slp.qty) : "[none]") << "\n";
            }
            if (on == slp.mint_baton_vout) {
                os
                    << "    mint_baton\n";
            }
        }

        if (tx.slp.type == gs::slp_transaction_type::mint) {
            const auto & slp = absl::get<gs::slp_transaction_mint>(tx.slp.slp_tx);
            if (on == 1) {
                os
                    << "    baton_vout:   " << slp.mint_baton_vout          << "\n"
                    << "    slp_amount:   " << slp.qty                      << "\n";
            }
            if (on == slp.mint_baton_vout) {
                os
                    << "    mint_baton\n";
            }
        }

        if (tx.slp.type == gs::slp_transaction_type::send) {
            const auto & slp = absl::get<gs::slp_transaction_send>(tx.slp.slp_tx);
            const std::uint64_t amnt = (on > 0 && on-1 < slp.amounts.size()) ? slp.amounts[on-1] : 0;
            os
                << "    slp_amount:   " << amnt << "\n";
        }

        os
            << "\n";

        ++on;
    }

    return os;
}
