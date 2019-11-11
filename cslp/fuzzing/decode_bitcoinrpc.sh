#!/bin/bash

if [ "$#" -ne 1 ]; then
    echo "wrong number of params, please pass a binary file"
fi

bitcoin-cli decoderawtransaction "`xxd -p -c10000000 ${1}`"

echo "exit code: $?"
