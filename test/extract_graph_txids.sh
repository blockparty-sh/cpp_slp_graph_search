#!/bin/bash

DB_NAME="slpdb"
MAX=1000000

while read TXID; do
    echo $TXID
done << EOF

$(mongo localhost/${DB_NAME}  --quiet --eval "DBQuery.shellBatchSize = ${MAX}; db.graphs.find({}, {\"graphTxn.txid\": 1, _id: 0}).limit(${MAX})" | jq -r ".graphTxn.txid")
EOF
