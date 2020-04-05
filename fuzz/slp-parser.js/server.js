/*
 * We use slp-validate's internal methods to get a JSON
 * Then we format this for consumption & comparison by differential fuzzer.
 */

const fs = require('fs');
const turbo = require('turbo-http')
const slpParser = require('slp-parser');

const server = turbo.createServer(function (req, res) {
    console.log(req.url);
    let y = null;
    try {
        const bin = Buffer.from(req.url[0] == '/' ? req.url.slice(1) : req.url, 'hex');

        y = slpParser.parseSLP(bin);
        y.versionType = y.tokenType;

        if (y.transactionType == "GENESIS") {
            y.symbol         = Buffer.from(y.data.ticker, 'binary').toString('hex').toUpperCase();
            y.name           = Buffer.from(y.data.name, 'binary').toString('hex').toUpperCase();
            y.documentUri    = Buffer.from(y.data.documentUri, 'binary').toString('hex').toUpperCase();
            if (y.data.documentHash === null) {
                y.documentSha256 = "";
            }
            y.documentSha256 = Buffer.from(y.data.documentHash, 'binary').toString('hex').toUpperCase();
        }

        if (y.data.hasOwnProperty("decimals")) {
            y.decimals = y.data.decimals;
        }

        if (y.data.hasOwnProperty("qty")) {
            y.genesisOrMintQuantity = y.data.qty.toString()
        }

        if (y.data.hasOwnProperty("tokenid")) {
            y.tokenIdHex = y.data.tokenid.toString('hex');
        }

        if (y.data.hasOwnProperty("mintBatonVout")) {
            y.batonVout = y.data.mintBatonVout;
        }

        if (y.data.hasOwnProperty("amounts")) {
            y.sendOutputs = y.data.amounts.map((v) => v.toString());
        }
     } catch(e) {
         const buf = Buffer.from(JSON.stringify({success: false, error: e.message}));
         res.setHeader('Content-Length', buf.length);
         res.write(buf);
         return;
     }

    const buf = Buffer.from(JSON.stringify({success: true, data: y}));
    res.setHeader('Content-Length', buf.length);
    res.write(buf);
    return;
});

server.listen(8078);
