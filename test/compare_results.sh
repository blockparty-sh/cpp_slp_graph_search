#!/bin/bash

GSPPCLI=../build/Release/gs++-cli
BASEURL="http://127.0.0.1:4000"
CACHEDIR="slpdb_cache_local"

while read TXID; do
    #TXID="b93ff39f13cecbfd7691ddc6c9c8f014b588c1abe1422122b35ee8369dfa3e16"
    B64_Q=$(cat slpdb_graph_search_query.json  | sed "s/REPLACER/${TXID}/g" | base64 -w 0)
    URL="${BASEURL}/q/${B64_Q}"

    if [ ! -f ./${CACHEDIR}/${TXID} ]
    then
        curl --silent ${URL} > ./${CACHEDIR}/${TXID}
    fi

    RES1=$(cat ./${CACHEDIR}/${TXID} | jq ".g | .[] | .dependsOn | sort | unique")

    # fix for empty arrays
    if [ "${RES1}" == "" ];
    then
        RES1="[]"
    fi

    RES2=$(${GSPPCLI} ${TXID} | jq --raw-input --slurp 'split("\n") | .[0:-1] | sort')
    RES1S=$(echo ${RES1} | sha256sum)
    RES2S=$(echo ${RES2} | sha256sum)


    if [ "${RES1S}" == "${RES2S}" ]; then
        echo "${TXID} -- passed"
    else
        echo "${TXID} -- failed"
        echo "$(echo ${RES1} | jq ".[]" | wc -l) -- $(echo ${RES2} | jq ".[]" | wc -l)"
        rm ./${CACHEDIR}/${TXID}
    fi
done <10000.txt
