#!/bin/sh

afl-fuzz -i corpus -o afl_out ./../../build-afl/cslp/fuzzing/cslp_fuzzing @@
