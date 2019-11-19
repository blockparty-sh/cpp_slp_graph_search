include(${CMAKE_SOURCE_DIR}/cmake/ProtobufGRPCGenerateCpp.cmake)

add_executable(gs++
    ${CMAKE_CURRENT_SOURCE_DIR}/gs++.cpp
    ${CMAKE_SOURCE_DIR}/src/mdatabase.cpp
    ${CMAKE_SOURCE_DIR}/src/txgraph.cpp
    ${CMAKE_SOURCE_DIR}/src/bch.cpp
    ${CMAKE_SOURCE_DIR}/src/utxodb.cpp
    ${CMAKE_SOURCE_DIR}/src/rpc.cpp
    ${CMAKE_SOURCE_DIR}/src/util.cpp
    ${CMAKE_SOURCE_DIR}/src/slp_transaction.cpp
    ${CMAKE_SOURCE_DIR}/src/sha2.cpp
    ${CMAKE_SOURCE_DIR}/src/block.cpp
    ${PROTO_SRCS}
    ${GRPC_SRCS}
)

target_include_directories(gs++ PUBLIC
    ${CMAKE_SOURCE_DIR}/include
    ${PROTO_SRC_DIR}
    ${LIBMONGOCXX_INCLUDE_DIRS}
    ${CMAKE_BINARY_DIR}/cpp-httplib/include
    ${CMAKE_BINARY_DIR}/nlohmann-json/include
)

target_link_libraries(gs++
    absl::flat_hash_map
    absl::node_hash_map
    absl::variant
    base64
    gRPC::grpc++_reflection
    protobuf::libprotobuf
    ${LIBMONGOCXX_LIBRARIES}
    ${CMAKE_THREAD_LIBS_INIT}
    spdlog
    mio
    httplib
    cppzmq
    ${Boost_FILESYSTEM_LIBRARY}
    ${Boost_SYSTEM_LIBRARY}
    ${Boost_THREAD_LIBRARY}
    ${Boost_OPTIONAL_LIBRARY}
)


target_compile_definitions(gs++ PUBLIC
    ${LIBMONGOCXX_DEFINITIONS}
)

target_compile_options(gs++ PRIVATE -Wall)
