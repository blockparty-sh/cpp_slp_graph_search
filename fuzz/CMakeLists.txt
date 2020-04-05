project(fuzz)

add_executable(fuzz_crash_slpparse
    ${CMAKE_CURRENT_SOURCE_DIR}/fuzzcrash_slpparse.cpp
    ${CMAKE_SOURCE_DIR}/src/transaction.cpp
    ${CMAKE_SOURCE_DIR}/src/slp_transaction.cpp
    ${CMAKE_SOURCE_DIR}/src/sha2.cpp
)

target_include_directories(fuzz_crash_slpparse PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(fuzz_crash_slpparse
    absl::variant
    absl::flat_hash_map
)

add_executable(fuzz_crash_bchparse
    ${CMAKE_CURRENT_SOURCE_DIR}/fuzzcrash_slpparse.cpp
    ${CMAKE_SOURCE_DIR}/src/transaction.cpp
    ${CMAKE_SOURCE_DIR}/src/slp_transaction.cpp
)

target_include_directories(fuzz_crash_bchparse PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(fuzz_crash_bchparse
    absl::variant
)

add_executable(fuzz_differential_nodejs
    ${CMAKE_CURRENT_SOURCE_DIR}/differential_nodejs.cpp
    ${CMAKE_SOURCE_DIR}/src/transaction.cpp
    ${CMAKE_SOURCE_DIR}/src/slp_transaction.cpp
    ${CMAKE_SOURCE_DIR}/src/sha2.cpp
)

target_include_directories(fuzz_differential_nodejs PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_BINARY_DIR}/nlohmann-json/include
    ${CMAKE_BINARY_DIR}/cpp-httplib/include
)

target_link_libraries(fuzz_differential_nodejs
    absl::variant
    absl::flat_hash_map
    absl::numeric
    absl::strings
    httplib
    ${Boost_FILESYSTEM_LIBRARY}
)

add_executable(fuzz_differential_slpindexer
    ${CMAKE_CURRENT_SOURCE_DIR}/differential_slpindexer.cpp
    ${CMAKE_SOURCE_DIR}/src/transaction.cpp
    ${CMAKE_SOURCE_DIR}/src/slp_transaction.cpp
    ${CMAKE_SOURCE_DIR}/src/sha2.cpp
)

target_include_directories(fuzz_differential_slpindexer PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_BINARY_DIR}/nlohmann-json/include
    ${CMAKE_BINARY_DIR}/cpp-httplib/include
)

target_link_libraries(fuzz_differential_slpindexer
    absl::variant
    absl::flat_hash_map
    absl::numeric
    absl::strings
    httplib
    ${Boost_FILESYSTEM_LIBRARY}
)

add_executable(fuzz_differential_python
    ${CMAKE_CURRENT_SOURCE_DIR}/differential_python.cpp
    ${CMAKE_SOURCE_DIR}/src/transaction.cpp
    ${CMAKE_SOURCE_DIR}/src/slp_transaction.cpp
    ${CMAKE_SOURCE_DIR}/src/sha2.cpp
)

target_include_directories(fuzz_differential_python PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_BINARY_DIR}/nlohmann-json/include
    ${CMAKE_BINARY_DIR}/cpp-httplib/include
)

target_link_libraries(fuzz_differential_python
    absl::variant
    absl::flat_hash_map
    absl::numeric
    absl::strings
    httplib
    ${Boost_FILESYSTEM_LIBRARY}
)


add_executable(fuzz_differential_bitcoinrpc
    ${CMAKE_CURRENT_SOURCE_DIR}/differential_bitcoinrpc.cpp
    ${CMAKE_SOURCE_DIR}/src/transaction.cpp
    ${CMAKE_SOURCE_DIR}/src/slp_transaction.cpp
    ${CMAKE_SOURCE_DIR}/src/sha2.cpp
    ${CMAKE_SOURCE_DIR}/src/rpc.cpp
)

target_include_directories(fuzz_differential_bitcoinrpc PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_BINARY_DIR}/nlohmann-json/include
    ${CMAKE_BINARY_DIR}/cpp-httplib/include
)

target_link_libraries(fuzz_differential_bitcoinrpc
    absl::variant
    absl::flat_hash_map
    absl::numeric
    absl::strings
    httplib
    ${Boost_FILESYSTEM_LIBRARY}
)

add_executable(fuzz_differential_slpparser
    ${CMAKE_CURRENT_SOURCE_DIR}/differential_slpparser.cpp
    ${CMAKE_SOURCE_DIR}/src/transaction.cpp
    ${CMAKE_SOURCE_DIR}/src/slp_transaction.cpp
    ${CMAKE_SOURCE_DIR}/src/sha2.cpp
)

target_include_directories(fuzz_differential_slpparser PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_BINARY_DIR}/nlohmann-json/include
    ${CMAKE_BINARY_DIR}/cpp-httplib/include
)

target_link_libraries(fuzz_differential_slpparser
    absl::variant
    absl::flat_hash_map
    absl::numeric
    absl::strings
    httplib
    ${Boost_FILESYSTEM_LIBRARY}
)
