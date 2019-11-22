# Fuzzing
 
These help to find issues missed by unit tests or discrepancies in implementation. 
 

# Building Fuzzers


```
mkdir build-afl
cd build-afl
cmake -DCMAKE_CXX_COMPILER=afl-clang-fast++ -DCMAKE_C_COMPILER=afl-clang-fast -Dgs++_BUILD_SERVER=OFF -Dgs++_BUILD_CLI=OFF -Dgs++_BUILD_TXDECODER=OFF -Dgs++_BUILD_TESTS=OFF -Dgs++_BUILD_FUZZ=ON ..
make -j
```

# Running

First we need to get some initial data to prime the fuzzer with. 

```
mkdir bchtx-corpus-pre
./build_bchtx_corpus.sh

mkdir slpscript-corpus-pre
./build_slpscript_corpus.sh
```

From here you can run the following fuzzers below. You may want to modify the scripts to run in [parallel](https://github.com/google/AFL/blob/master/docs/parallel_fuzzing.txt).


## BCH Transaction Parser

This will look for crashes in BCH transaction parsing.

```
./fuzzcrash-bchtx.sh
```

## SLP Transaction Parser

This will look for crashes in SLP script parsing.

```
./fuzzcrash-slpscript.sh
```


## Bitcoind Differential

This tests bch tx parsing against bitcoind decoderawtransaction parser.

```
cp rpc_config.example.hpp rpc_config.hpp
$EDITOR rpc_config.hpp
./diff-bitcoind.sh

```


## Node.js Differential

This tests SLP transaction parsing against [slp-validate](https://github.com/simpleledger/slp-validate) parser.


```
cd nodejs_validation
npm install
npm start

# open another terminal
cd ..
./diff-nodejs.sh
```

## Python Differential

This tests SLP transaction parsing against [Electron Cash SLP Edition](https://github.com/simpleledger/Electron-Cash-SLP) parser.

```
cd python_validation
ln -s /home/user/Electron-Cash-SLP/lib .
pip3 install sanic
python3 server.py

# open another terminal
cd ..
./diff-nodejs.sh
```
