#!/bin/sh

afl-fuzz -i corpus -x slp.dict -o out/diff-nodejs -m1000 -t1000 ./../../build-afl/cslp/fuzzing/cslp_differential_nodejs @@
