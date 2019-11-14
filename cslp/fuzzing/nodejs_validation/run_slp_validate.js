/*
 * We use slp-validate's internal methods to get a JSON
 * Then we format this for consumption & comparison by differential fuzzer.
 */

const fs = require('fs');
const turbo = require('turbo-http')
const validate = require('slp-validate');

const server = turbo.createServer(function (req, res) {
    console.log(req.url);
    let y = null;
    try {
        const bin = Buffer.from(req.url[0] == '/' ? req.url.slice(1) : req.url, 'hex');

        y = validate.Slp.parseSlpOutputScript(bin);

        if (y.transactionType == "GENESIS") {
            y.symbol         = Buffer.from(y.symbol, 'binary').toString('hex').toUpperCase();
            y.name           = Buffer.from(y.name, 'binary').toString('hex').toUpperCase();
            y.documentUri    = Buffer.from(y.documentUri, 'binary').toString('hex').toUpperCase();
            if (y.documentSha256 === null) {
                y.documentSha256 = "";
            }
            y.documentSha256 = Buffer.from(y.documentSha256, 'binary').toString('hex').toUpperCase();
        }

        if (y.hasOwnProperty("genesisOrMintQuantity")) {
            y.genesisOrMintQuantity = y.genesisOrMintQuantity.toString()
        }

        if (y.hasOwnProperty("sendOutputs")) {
            y.sendOutputs = y.sendOutputs.map((v) => v.toString());
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

server.listen(8077);
