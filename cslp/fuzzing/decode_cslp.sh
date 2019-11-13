#!/bin/bash

if [ "$#" -ne 1 ]; then
    echo "wrong number of params, please pass a binary file"
fi

../../build-afl/cslp/fuzzing/cslp_fuzzcrash_slpparse ${1}

echo "exit code: $?"
