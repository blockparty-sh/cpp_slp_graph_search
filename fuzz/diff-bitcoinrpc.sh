#!/bin/sh

afl-fuzz -i bchtx-corpus -x slp.dict -o out/diff-bitcoind -m1000 -t1000 ./../build-afl/bin/fuzz_differential_bitcoinrpc @@
