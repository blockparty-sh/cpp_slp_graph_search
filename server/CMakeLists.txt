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
    ${PROTO_SRCS}
    ${GRPC_SRCS}
)

target_include_directories(gs++ PUBLIC
    ${CMAKE_SOURCE_DIR}/include
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
    ${CMAKE_THREAD_LIBS_INIT}
    ${Boost_FILESYSTEM_LIBRARY}
    ${Boost_SYSTEM_LIBRARY}
    ${Boost_THREAD_LIBRARY}
    ${Boost_OPTIONAL_LIBRARY}
)
