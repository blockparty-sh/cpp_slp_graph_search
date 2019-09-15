#!/bin/bash

GSPPCLI=../build/Release/gs++-cli

while read TXID; do
    B64_Q=$(cat slpdb_graph_search_query.json  | sed "s/REPLACER/${TXID}/g" | base64 -w 0)
    URL="https://slpdb.fountainhead.cash/q/${B64_Q}"

    if [ ! -f ./slpdb_cache/${TXID} ]
    then
        curl --silent ${URL} > ./slpdb_cache/${TXID}
    fi

    RES1=$(cat ./slpdb_cache/${TXID} | jq ".g | .[] | .dependsOn | sort")
    RES2=$(${GSPPCLI} ${TXID} | jq --raw-input --slurp 'split("\n") | .[1:-1] | sort')
    RES1S=$(echo ${RES1} | sha256sum)
    RES2S=$(echo ${RES2} | sha256sum)

    if [ "${RES1S}" == "${RES2S}" ]; then
        echo "${TXID} -- passed"
    else
        echo "${TXID} -- failed"
    fi
done <10000.txt
