gs++ | SLP Graph Search Server
===

WIP

In-memory SLP Graph Search server. 

# Install



## Install Third Party Libs

Install recent version of [Boost](https://www.boost.org/)

Install [SWIG 3](http://www.swig.org)

Follow [this](https://github.com/grpc/grpc/blob/master/BUILDING.md) guide to install gRPC and Protobuf for your system if it is not already installed.


## Build

The initial build might take a long time to download and build required dependencies, so grab some coffee.

From root directory build like normal CMake program.

```
mkdir build
cd build
cmake ..
make -j
```

You can use `-DCMAKE_BUILD_TYPE=Debug` for debug build.


# Running

Make sure you have bitcoind running and the following options enabled:

```
txindex=1
server=1
rpcuser=bitcoin
rpcpassword=password
rpcport=8332
rpcworkqueue=10000
rpcthreads=8
zmqpubhashtx=tcp://*:28332
zmqpubrawtx=tcp://*:28332
zmqpubhashblock=tcp://*:28332
zmqpubrawblock=tcp://*:28332
```

## gs++

This is the server portion. You will pass a config file to it as the only argument.

`./bin/gs++ ../config.toml`

## gs++-cli

You can query the server using the cli program to test.

`./bin/gs++-cli --validate 508e543ff30ffe670e30ebd281ab25ebe6767071e87decbb958230a7760936ae`

## txdecoder

This is a small utility to debug SLP transactions. Just pass the txdata as hex as the only argument.

`./bin/txdecoder 0200000002168bef68766234af97c65c60f2891be0bc8bbc2894f6517b9a216da95ba12c1f020000006a47304402206fcbf79712d6c84f4367c0ba6d7ec27f4d4690dd65d8c4e49cfe7f543a515736022007d7b4fb3f6315e432c673a8793dc1a7ccbd1e73eeba6498bb636ed409daa0c941210350090260acd0cd7a5f7030b2aad76ec6454626ab0872246031f3809d210e4569ffffffff168bef68766234af97c65c60f2891be0bc8bbc2894f6517b9a216da95ba12c1f030000006b483045022100dabdf03df22031dc386761aa40e15dbb654a42e8dc27803f2de9eda2651236cd02205321628ad9d75dc76d16365cb77c2fa75d03a2497a116b23230eebed2f23f9d741210350090260acd0cd7a5f7030b2aad76ec6454626ab0872246031f3809d210e4569ffffffff040000000000000000406a04534c500001010453454e44207f8889682d57369ed0e32336f8b7e0ffec625a35cca183f4e81fde4e71a538a1080000000000000cdb08000000001936f55b22020000000000001976a9140315c06540c4445792ea3ff407a978ec18da1d8188ac22020000000000001976a9144942f11b739a3835d867554ff93cc6685eb1eb5388ac5fb27100000000001976a9144942f11b739a3835d867554ff93cc6685eb1eb5388ac00000000`

## blockdecoder

This is a small utility to debug cached blocks. Just pass the blockdata as hex as the only argument.

```
./bin/blockdecoder `xxd -c10000000000 -p ../cache/slp/610/610104`
```

## unit tests

We have a few unit tests, please help add to these. Many of the tests come from the [slp-unit-test-data](https://github.com/simpleledger/slp-unit-test-data) repository.

`./bin/unit-test`

# Fuzzing

Please read the [README](./fuzz/README.md) which will describe how to set this up.


## REST

There is also a simple JSON server in `./rest` which both shows how to use this as well as is an alternative to connecting via gRPC.

# Integration

You can use any grpc client to connect to a running server. It is recommended you disable max message size. Use the definitions in the `./pb` directory.

# TODO

set up ubsan

build with "-fsanitize=address" and/or "-fsanitize=undefined"

gs++:
    look into sending txs and early exit based on that

add grpc queries for
    look up slp utxos by scriptpubkey
    look up slp utxos for a specific token by scriptpubkey
    look up all utxos for a specific token by scriptpubkey
    look up slp utxo
    look up slp token (stats/details + minting baton)

rollback using transactions instead of input/output
    rollback slp as well

track block headers so we know when to automatically rollback

evict mempool items if new tx with same input used
    this could be a chain of transactions so must recurse
    this should also be done during new block processing

query rpc for mempool items on startup to add

rest:
    add broadcast endpoint
    add gettxproof endpoint

    look up slp utxos by scriptpubkey
    look up slp utxos for a specific token by scriptpubkey
    look up all utxos for a specific token by scriptpubkey
    look up slp utxo
    look up slp token (stats/details + minting baton)


swig:
    set up targets to build cslp wrappers for variety of languages
