#!/bin/sh

afl-fuzz -i slpscript-corpus -x slp.dict -o out/diff-python -m1000 -t10000 ./../build-afl/bin/fuzz_differential_python @@
