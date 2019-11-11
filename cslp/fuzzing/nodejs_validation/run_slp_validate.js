const fs = require('fs');
const validate = require('slp-validate');

if (process.argv.length < 3) {
    console.log('missing input file');
    process.exit(1);
}

bin = fs.readFileSync(process.argv[2])

try {
    x = validate.Transaction.parseFromBuffer(bin);
} catch {
    process.exit(1);
}

try {
    y = validate.Slp.parseSlpOutputScript(x.outputs[0].scriptPubKey);

    if (y.transactionType === "GENESIS" || y.transactionType === "MINT") {
        y.genesisOrMintQuantity = y.genesisOrMintQuantity.toString()
    }

    if (y.transactionType === "SEND") {
        y.sendOutputs = y.sendOutputs.map((v) => v.toString());
    }
} catch {
    y = null;
}

x.slp = y;


console.log(JSON.stringify(x));
