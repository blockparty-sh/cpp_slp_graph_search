#!/bin/bash

./download_test_cases_txids.py | while read txid;
do
    bitcoin-cli getrawtransaction ${txid} | xxd -r -p - corpus-pre/${txid}.tx;
done

afl-cmin -i bchtx-corpus-pre -o bchtx-corpus -- ./../../build-afl/cslp/fuzzing/cslp_fuzzing @@
