#!/bin/bash

# does 10000 graph searches

GSPPCLI=../build/gs++-cli

while read TXID; do
    ${GSPPCLI} ${TXID} > /dev/null &
done <10000.txt
