const btoa = require('btoa');
const fetch = require('node-fetch');

const slpdb = {
  query: (query, verbose=false) => new Promise((resolve, reject) => {
    if (! query) {
      return resolve(false);
    }
    const b64 = btoa(JSON.stringify(query));
    const url = "http://127.0.0.1:4000/q/" + b64;

    if (verbose) console.log(url)

    fetch(url)
    .then((r) => r = r.json())
    .then((r) => {
      if (r.hasOwnProperty('error')) {
        reject(new Error(r['error']));
      }
      resolve(r);
    });
  }),
};


const graph_search = (txid, txidset, rawmap) => slpdb.query(
	{
		"v": 3,
		"q": {
			"db": ["g"],
			"aggregate": [
				{
					"$match": {
						"graphTxn.txid": txid
					}
				},
				{
					"$lookup": {
						"from": "confirmed",
						"localField": "graphTxn.txid",
						"foreignField": "tx.h",
						"as": "tx"
					}
				}
			]
		},
		"limit": 10
	}
).then((data) => new Promise((resolve, reject) => {
    if (! data.g) return;
	if (data.g.length    === 0) return;

	const g     = data.g[0];
	if (g.tx.length === 0) return;
	const txid  = g.tx[0].tx.h;
	const rawtx = g.tx[0].tx.raw;

	txidset.add(txid);
	rawmap.set(txid, rawtx);

    console.log(txid);

    let tasks = [];
	for (let m of g.graphTxn.inputs) {
		if (txidset.has(m.txid)) {
			continue;
		}

        tasks.push(graph_search(m.txid, txidset, rawmap));
	}

    return Promise.all(tasks).then(() => {
        resolve(rawmap);
    });
}));


graph_search('5c882926a710161b7e5eed6b8c7d4a942a82e2f7901cac68d37dba354153d5be', new Set(), new Map())
.then((rawmap) => {
    console.log(rawmap.values());
});
