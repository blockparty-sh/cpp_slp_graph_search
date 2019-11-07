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

    void cslp_validator_add_tx(cslp_validator validator, const char * txdata)
    {
        gs::slp_validator * slp_validator = static_cast<gs::slp_validator*>(validator);
        gs::transaction tx(txdata, 0);
        slp_validator->add_tx(tx);
    }

    void cslp_validator_remove_tx(cslp_validator validator, const char * txid)
    {
        gs::slp_validator * slp_validator = static_cast<gs::slp_validator*>(validator);
        slp_validator->remove_tx(gs::txid(std::string(txid)));
    }

    int cslp_validator_validate(cslp_validator validator, const char * txid)
    {
        gs::slp_validator * slp_validator = static_cast<gs::slp_validator*>(validator);
        bool valid = slp_validator->validate(gs::txid(std::string(txid)));
        return valid;
    }

    void cslp_validator_destroy(cslp_validator validator)
    {
        gs::slp_validator * slp_validator = static_cast<gs::slp_validator*>(validator);
        delete slp_validator;
    }
}

