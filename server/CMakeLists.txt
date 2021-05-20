include(${CMAKE_SOURCE_DIR}/cmake/ProtobufGRPCGenerateCpp.cmake)

add_executable(gs++
    ${CMAKE_CURRENT_SOURCE_DIR}/gs++.cpp
    ${CMAKE_SOURCE_DIR}/src/txgraph.cpp
    ${CMAKE_SOURCE_DIR}/src/bch.cpp
    ${CMAKE_SOURCE_DIR}/src/utxodb.cpp
    ${CMAKE_SOURCE_DIR}/src/rpc.cpp
    ${CMAKE_SOURCE_DIR}/src/util.cpp
    ${CMAKE_SOURCE_DIR}/src/transaction.cpp
    ${CMAKE_SOURCE_DIR}/src/slp_transaction.cpp
    ${CMAKE_SOURCE_DIR}/src/sha2.cpp
    ${CMAKE_SOURCE_DIR}/src/block.cpp
    ${CMAKE_SOURCE_DIR}/src/slp_validator.cpp
    ${CMAKE_SOURCE_DIR}/src/secp256k1/secp256k1.c
    ${CMAKE_SOURCE_DIR}/src/cashaddr/cashaddr.cpp
    ${PROTO_SRCS}
    ${GRPC_SRCS}
)

target_compile_definitions(gs++
    PUBLIC ENABLE_MODULE_SCHNORR
    PUBLIC USE_NUM_GMP
    PUBLIC USE_FIELD_10X26
    PUBLIC USE_FIELD_INV_BUILTIN
    PUBLIC USE_SCALAR_8X32
    PUBLIC USE_SCALAR_INV_BUILTIN
    PUBLIC ECMULT_WINDOW_SIZE=20
    PUBLIC ECMULT_GEN_PREC_BITS=4
)

target_include_directories(gs++ PUBLIC
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include/3rdparty/secp256k1
    ${CMAKE_SOURCE_DIR}/include/3rdparty/cashaddr
    ${CMAKE_SOURCE_DIR}/src/secp256k1
    ${PROTO_SRC_DIR}
    ${CMAKE_BINARY_DIR}/cpp-httplib/include
    ${CMAKE_BINARY_DIR}/nlohmann-json/include
    ${CMAKE_BINARY_DIR}/toml11/include
)

target_link_libraries(gs++
    absl::flat_hash_map
    absl::node_hash_map
    absl::variant
    base64
    gRPC::grpc++_reflection
    protobuf::libprotobuf
    spdlog
    httplib
    cppzmq
    atomic
    gmp
    ${CMAKE_THREAD_LIBS_INIT}
    ${Boost_FILESYSTEM_LIBRARY}
    ${Boost_SYSTEM_LIBRARY}
    ${Boost_THREAD_LIBRARY}
    ${Boost_OPTIONAL_LIBRARY}
)
