#!/bin/sh

afl-fuzz -i corpus -o afl_differential_out -m1000 -t1000 ./../../build-afl/cslp/fuzzing/cslp_differential @@
