typedef void * cslp_validator;

#ifdef __cplusplus
extern "C" {
#endif

cslp_validator cslp_validator_init();
void cslp_validator_add_tx(cslp_validator validator, const char * txdata, int txdata_len);
void cslp_validator_remove_tx(cslp_validator validator, const char * txid);
int cslp_validator_validate(cslp_validator validator, const char * txid);
void cslp_validator_destroy(cslp_validator validator);

#ifdef __cplusplus
}
#endif

