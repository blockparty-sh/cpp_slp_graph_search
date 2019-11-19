#include <cslp/cslp.h>


int main(int argc, char *argv[])
{
    cslp_validator validator = cslp_validator_init();
    const char * txid = "\01\01\01\01\01\01\01\01\01\01\01\01\01\01\01\01\01\01\01\01\01\01\01\01\01\01\01\01\01\01\01\01";
    cslp_validator_validate_txid(validator, txid);
    cslp_validator_destroy(validator);


    return 0;
}
