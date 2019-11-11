#!/bin/sh

afl-fuzz -n -i afl_in -o afl_out ./../../build/cslp/fuzzing/cslp_fuzzing @@
