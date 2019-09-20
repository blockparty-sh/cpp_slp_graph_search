require('dotenv').config()

const grpc = require('grpc');
const graphsearch = require('./pb/graphsearch_pb.js');
const graphsearch_service = require('./pb/graphsearch_grpc_pb.js');

const express = require('express')
const app = express()
const cors = require("cors")

const client = new graphsearch_service.GraphSearchServiceClient(
  process.env.graphsearch_grpc_server_bind,
  grpc.credentials.createInsecure()
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

app.listen(process.env.graphsearch_http_port, () => {
  console.log(`graphsearch rest server started on port ${process.env.graphsearch_http_port}`);
});
