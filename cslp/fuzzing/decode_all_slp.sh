#!/bin/bash

if [ "$#" -ne 1 ]; then
    echo "wrong number of params, please pass a binary file"
fi

echo "cslp"
printf "\t"
./decode_cslp.sh ${1} 2> /dev/null

echo "nodejs"
printf "\t"
./decode_nodejs.sh ${1} 2> /dev/null
