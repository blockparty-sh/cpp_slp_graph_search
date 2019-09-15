#!/bin/bash

# does 10000 graph searches

GSPPCLI=../build/Release/gs++-cli

while read TXID; do
    ${GSPPCLI} ${TXID} > /dev/null &
done <10000.txt
