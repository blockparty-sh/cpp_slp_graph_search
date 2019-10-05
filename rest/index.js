require('dotenv').config()

const grpc = require('grpc');
const graphsearch = require('./pb/graphsearch_pb.js');
const graphsearch_service = require('./pb/graphsearch_grpc_pb.js');

const express = require('express')
const app = express()
const cors = require("cors")

const bitcore = require('bitcore-lib-cash');
const cashaddrjs = require('cashaddrjs');

const client = new graphsearch_service.GraphSearchServiceClient(
  process.env.graphsearch_grpc_server_bind,
  grpc.credentials.createInsecure(),
  {
     'grpc.max_receive_message_length': 1024*1024*1024
  },
);

app.use(cors())
app.enable("trust proxy")
app.get(/^\/slp\/graphsearch\/(.+)/, function(req, res) {
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

app.get(/^\/utxo\/(.+)/, function(req, res) {
  const outpoints = req.params[0].split(',');

  const request = new graphsearch.UtxoSearchByOutpointsRequest();
  for (const o of outpoints) {
      const segments = o.split(':');

      if (segments.length !== 2 || isNaN(segments[1])) {
        res.end(JSON.stringify({
          success: false,
          error:   "bad format"
        }));
        return;
      }

      const outpoint = request.addOutpoints();
      outpoint.setTxid(bytesToHex(hexToBytes(segments[0]).reverse()));
      outpoint.setVout(segments[1]);
  }

  
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
    console.log('utxo', req.params[0], outputslist.length);

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
        } catch (e) {
          ret['address'] = null;
        }

        return ret;
      }),
	}));
  });
});

app.get(/^\/address\/utxos\/(.+)/, function(req, res) {
  const scriptpubkey = addressToScriptpubkey(req.params[0]);

  const request = new graphsearch.UtxoSearchByScriptPubKeyRequest();
  request.setScriptpubkey(scriptpubkey);
  request.setLimit(10000);
  
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

app.get(/^\/address\/balance\/(.+)/, function(req, res) {
  const scriptpubkey = addressToScriptpubkey(req.params[0]);

  const request = new graphsearch.BalanceByScriptPubKeyRequest();
  request.setScriptpubkey(scriptpubkey);
  
  client.balanceByScriptPubKey(request, function(err, response) {
    res.setHeader('Content-Type', 'application/json');
    if (err) {
      console.log(err);
      res.end(JSON.stringify({
        success: false,
        error:   err
      }));
      return;
    }

    const balance = response.getBalance();
    console.log(scriptpubkey, balance);

    res.end(JSON.stringify({
      success: true,
      data: balance,
    }));
  });
});

app.listen(process.env.graphsearch_http_port, () => {
  console.log(`graphsearch rest server started on port ${process.env.graphsearch_http_port}`);
});

const hexToBytes = (hex) => {
  let bytes = [];

  for (let c=0; c < hex.length; c += 2) {
    bytes.push(parseInt(hex.substr(c, 2), 16));
  }

  return bytes;
};

const bytesToHex = (bytes) => {
  let hex = [];

  for (let i=0; i < bytes.length; i++) {
    const current = bytes[i] < 0 ? bytes[i] + 256 : bytes[i];
    hex.push((current >>> 4).toString(16));
    hex.push((current & 0xF).toString(16));
  }

  return hex.join("");
};

const addressToScriptpubkey = (address) => {
  const x = cashaddrjs.decode(address);
  return Buffer.from(
      (x.type === 'P2PKH')
    ? [0x76, 0xA9, x.hash.length].concat(...x.hash, [0x88, 0xAC])
    : (x.type === 'P2PK')
    ? [0xAC, x.hash.length].concat(...x.hash, [0x87])
    : [0xA9, x.hash.length].concat(...x.hash, [0x87]) // assume p2sh
  ).toString('base64');
};
