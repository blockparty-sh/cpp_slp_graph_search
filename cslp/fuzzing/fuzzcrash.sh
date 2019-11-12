#!/bin/sh

afl-fuzz -i corpus -x slp.dict -o out/fuzzcrash_slpparse -m1000 -t1000 ./../../build-afl/cslp/fuzzing/cslp_fuzzcrash_slpparse @@
