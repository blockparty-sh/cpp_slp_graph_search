#!/bin/bash

cd third-party
git clone https://github.com/abseil/abseil-cpp.git
git clone https://github.com/BurningEnlightenment/base64-cmake base64

git clone -b $(curl -L http://grpc.io/release) https://github.com/grpc/grpc
cd grpc
git submodule update --init
make HAS_SYSTEM_PROTOBUF=false
sudo make install

cd third_party/protobuf
make
sudo make install

cd ../../..
mkdir -p build/Debug
cd build/Debug
cmake ..
make

cd ..
mkdir -p build/Release
cd build/Release
cmake -DCMAKE_BUILD_TYPE=Release ..
make
