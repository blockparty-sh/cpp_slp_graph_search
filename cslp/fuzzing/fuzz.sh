#!/bin/sh

afl-fuzz -i afl_in -o afl_out ./../build/cslp_fuzzing @@
