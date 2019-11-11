#!/bin/bash

if [ "$#" -ne 1 ]; then
    echo "wrong number of params, please pass a binary file"
fi

node nodejs_validation/run_slp_validate.js ${1}

echo "exit code: $?"
