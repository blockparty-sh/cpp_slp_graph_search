gs++ | SLP Graph Search Server
===

WIP

In-memory SLP Graph Search server. 

# Install

## SLPDB

Install [SLPDB](https://github.com/simpleledger/SLPDB) and sync it (might take a while).

## Install Third Party Libs

Follow [this](https://github.com/grpc/grpc/blob/master/BUILDING.md) guide to install gRPC and Protobuf for your system if it is not already installed.


## Build

From root directory build like normal CMake program.

```
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make -j
```

You can remove `-DCMAKE_BUILD_TYPE=Release` for debug build.


# Running

Just view the help for the programs. Configure them to use your SLPDB mongo database.

`./gs++ -h`

After reading help, start server:

`./gs++`

Then query it:

`./gs++-cli 508e543ff30ffe670e30ebd281ab25ebe6767071e87decbb958230a7760936ae`

# Fuzzing

You will need to first install afl to run fuzzing over the `cslp` library.

```
mkdir build-afl
cd build-afl
cmake -DCMAKE_CXX_COMPILER=afl-clang-fast++ -DCMAKE_C_COMPILER=afl-clang-fast ..
make -j
make cslp_fuzzing
cd ../cslp/fuzzing
cd nodejs_validation
npm install

# this will check for crashes of cslp library
./fuzz-instrumented.sh

# this will compare outputs between our parser and slp-validate nodejs library
./fuzz-differential.sh
```


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
