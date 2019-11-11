add_executable(internals_test
    ${CMAKE_CURRENT_SOURCE_DIR}/internals_test.cpp
    ${CMAKE_SOURCE_DIR}/src/utxodb.cpp
    ${CMAKE_SOURCE_DIR}/src/bch.cpp
    ${CMAKE_SOURCE_DIR}/src/util.cpp
    ${CMAKE_SOURCE_DIR}/src/slp_transaction.cpp
    ${CMAKE_SOURCE_DIR}/src/sha2.cpp
)

target_include_directories(internals_test PUBLIC
    ${CMAKE_SOURCE_DIR}/include
    ${CATCH_INCLUDE_DIR}
    ${CMAKE_BINARY_DIR}/cpp-httplib/include
    ${CMAKE_BINARY_DIR}/nlohmann-json/include
)

target_link_libraries(internals_test
    Catch2::Catch2
    absl::flat_hash_map
    absl::node_hash_map
    base64
    mio
    spdlog
    ${Boost_THREAD_LIBRARY}
)
