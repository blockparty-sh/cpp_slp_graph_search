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


# Integration

You can use any grpc client to connect to a running server. It is recommended you disable max message size. Use the definitions in the `./pb` directory.
