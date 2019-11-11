#!/bin/sh

afl-fuzz -i corpus -x slp.dict -o out/diff-bitcoind -m1000 -t1000 ./../../build-afl/cslp/fuzzing/cslp_differential_bitcoinrpc @@
