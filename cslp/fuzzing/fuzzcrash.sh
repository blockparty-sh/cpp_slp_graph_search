#!/bin/sh

afl-fuzz -i corpus -x slp.dict -o out/fuzzcrash ./../../build-afl/cslp/fuzzing/cslp_fuzzcrash @@
