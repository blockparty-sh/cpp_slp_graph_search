#!/bin/bash

if [ "$#" -ne 1 ]; then
    echo "wrong number of params, please pass a binary file"
fi

node --no-warnings nodejs_validation/report.js ${1}

echo "exit code: $?"
