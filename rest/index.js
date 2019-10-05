require('dotenv').config()

const grpc = require('grpc');
const graphsearch = require('./pb/graphsearch_pb.js');
const graphsearch_service = require('./pb/graphsearch_grpc_pb.js');

const express = require('express')
const app = express()
const cors = require("cors")

const bitcore = require('bitcore-lib-cash');

const client = new graphsearch_service.GraphSearchServiceClient(
  process.env.graphsearch_grpc_server_bind,
  grpc.credentials.createInsecure(),
  {
     'grpc.max_receive_message_length': 1024*1024*1024
  },
);

app.use(cors())
app.enable("trust proxy")
app.get(/^\/graphsearch\/(.+)/, function(req, res) {
  const txid = req.params[0]

  const request = new graphsearch.GraphSearchRequest();
  request.setTxid(txid);
  
  client.graphSearch(request, function(err, response) {
    res.setHeader('Content-Type', 'application/json');
    if (err) {
      console.log(err);
      res.end(JSON.stringify({
        success: false,
        error:   err
      }));
      return;
    }

    const txdatalist = response.getTxdataList_asB64();
    console.log(txid, txdatalist.length);

    res.end(JSON.stringify({
      success: true,
      data:    txdatalist
    }));
  });
});

app.get(/^\/utxo\/(.+)\/(.+)/, function(req, res) {
  const txid = bytesToHex(hexToBytes(req.params[0]).reverse());
  const vout = req.params[1];

  const request = new graphsearch.UtxoSearchByOutpointsRequest();
  let outpoint = request.addOutpoints();
  outpoint.setTxid(txid);
  outpoint.setVout(vout);
  
  client.utxoSearchByOutpoints(request, function(err, response) {
    res.setHeader('Content-Type', 'application/json');
    if (err) {
      console.log(err);
      res.end(JSON.stringify({
        success: false,
        error:   err
      }));
      return;
    }

    const outputslist = response.getOutputsList();
    console.log(txid, vout, outputslist.length);

    res.end(JSON.stringify({
      success: true,
      data:    outputslist.map(v => {
        let ret = {
          prevTxId:   bytesToHex(Object.values(v.getPrevTxId_asU8()).reverse()),
          prevOutIdx: v.getPrevOutIdx(),
          height:     v.getHeight(),
          value:      v.getValue(),
          pkScript:   v.getScriptpubkey_asB64()
        };

        try {
          ret['address'] = bitcore.Address.fromScript(
            bitcore.Script(Buffer.from(ret['pkScript'], 'base64'))
          ).toString();
        } catch (e) {}

        return ret;
      }),
	}));
  });
});

app.get(/^\/utxo_scriptpubkey\/(.+)/, function(req, res) {
  const scriptpubkey = req.params[0];

  const request = new graphsearch.UtxoSearchByScriptPubKeyRequest();
  request.setScriptpubkey(scriptpubkey);
  
  client.utxoSearchByScriptPubKey(request, function(err, response) {
    res.setHeader('Content-Type', 'application/json');
    if (err) {
      console.log(err);
      res.end(JSON.stringify({
        success: false,
        error:   err
      }));
      return;
    }

    const outputslist = response.getOutputsList();
    console.log(scriptpubkey, outputslist.length);

    res.end(JSON.stringify({
      success: true,
      data:    outputslist.map(v => ({
        prevTxId:   bytesToHex(Object.values(v.getPrevTxId_asU8()).reverse()),
        prevOutIdx: v.getPrevOutIdx(),
        height:     v.getHeight(),
        value:      v.getValue(),
      })),
    }));
  });
});

app.listen(process.env.graphsearch_http_port, () => {
  console.log(`graphsearch rest server started on port ${process.env.graphsearch_http_port}`);
});

// Convert a hex string to a byte array
function hexToBytes(hex) {
    for (var bytes = [], c = 0; c < hex.length; c += 2)
    bytes.push(parseInt(hex.substr(c, 2), 16));
    return bytes;
}

// Convert a byte array to a hex string
function bytesToHex(bytes) {
    for (var hex = [], i = 0; i < bytes.length; i++) {
        var current = bytes[i] < 0 ? bytes[i] + 256 : bytes[i];
        hex.push((current >>> 4).toString(16));
        hex.push((current & 0xF).toString(16));
    }
    return hex.join("");
}

