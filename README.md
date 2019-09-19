gs++ | SLP Graph Search Server
===

WIP

In-memory SLP Graph Search server. 

# Install

## SLPDB

Install [SLPDB](https://github.com/simpleledger/SLPDB) and sync it (might take a while).

## Install Third Party Libs

```

cd third-party
git clone -b $(curl -L http://grpc.io/release) https://github.com/grpc/grpc
cd grpc
git submodule update --init
make HAS_SYSTEM_PROTOBUF=false
sudo make install
cd third_party/protobuf
make
sudo make install

```

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


# Integration

You can use any grpc client to connect to a running server. It is recommended you disable max message size. Use the definitions in the `./pb` directory.
