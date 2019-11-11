#!/usr/bin/env python3

import urllib.request
from base64 import b64encode
import json


def download_type(find_q, limit=1):
    query = {
      "v": 3,
      "q": {
        "db": ["c"],
        "find": find_q,
        "project": {
          "tx.h": 1
        },
        "limit": limit
      },
      "r": {
        "f": "[ .[] | { txid: .tx.h } ]"
      }
    }

    # python3 was a mistake
    url = 'https://slpdb.fountainhead.cash/q/' + str(b64encode(bytes(json.dumps(query), 'ascii')), 'ascii')

    req = urllib.request.Request(url)
    #
    r = urllib.request.urlopen(req).read()
    cont = json.loads(r.decode('ascii'))

    for item in cont['c']:
        print(item['txid'])


download_type({"slp.valid": False })
download_type({"slp.valid": True, "slp.detail.transactionType": "GENESIS" })
download_type({"slp.valid": True, "slp.detail.transactionType": "MINT" })
download_type({"slp.valid": True, "slp.detail.transactionType": "SEND" })
download_type({}, 10000)
