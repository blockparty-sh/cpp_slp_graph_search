#include <cslp/cslp.h>
#include <gs++/transaction.hpp>
#include <gs++/slp_validator.hpp>
#include <gs++/bhash.hpp>

extern "C" {
    cslp_validator cslp_validator_init()
    {
        gs::slp_validator * slp_validator = new gs::slp_validator();
        return static_cast<cslp_validator>(slp_validator);
    }

    void cslp_validator_add_tx(cslp_validator validator, const char * txdata, int txdata_len)
    {
        gs::slp_validator * slp_validator = static_cast<gs::slp_validator*>(validator);
        gs::transaction tx;
        tx.hydrate(txdata, txdata+txdata_len);
        slp_validator->add_tx(tx);
    }

    void cslp_validator_remove_tx(cslp_validator validator, const char * txid)
    {
        gs::slp_validator * slp_validator = static_cast<gs::slp_validator*>(validator);
        slp_validator->remove_tx(gs::txid(std::string(txid)));
    }

    int cslp_validator_validate_txid(cslp_validator validator, const char * txid)
    {
        gs::slp_validator * slp_validator = static_cast<gs::slp_validator*>(validator);
        bool valid = slp_validator->validate(gs::txid(std::string(txid)));
        return valid;
    }

    int cslp_validator_validate_tx(cslp_validator validator, const char * txdata, int txdata_len)
    {
        gs::slp_validator * slp_validator = static_cast<gs::slp_validator*>(validator);
        gs::transaction tx;
        if (! tx.hydrate(txdata, txdata+txdata_len)) {
            return false;
        }
        bool valid = slp_validator->validate(tx);
        return valid;
    }

    void cslp_validator_destroy(cslp_validator validator)
    {
        gs::slp_validator * slp_validator = static_cast<gs::slp_validator*>(validator);
        delete slp_validator;
    }
}

