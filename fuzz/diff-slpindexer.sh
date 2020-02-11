#!/bin/sh

afl-fuzz -i slpscript-corpus -x slp.dict -o out/diff-slpindexer -m1000 -t1000 ./../build-afl/bin/fuzz_differential_slpindexer @@
