#!/bin/sh

afl-fuzz -i bchtx-corpus -x slp.dict -o out/fuzzcrash_bchparse -m1000 -t10 ./../../build-afl/cslp/fuzzing/cslp_fuzzcrash_bchparse @@
