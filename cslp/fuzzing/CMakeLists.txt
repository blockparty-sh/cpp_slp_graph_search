project(cslp_fuzzing)

add_executable(cslp_fuzzcrash_slpparse
    ${CMAKE_CURRENT_SOURCE_DIR}/fuzzcrash_slpparse.cpp
    ${CMAKE_SOURCE_DIR}/src/slp_transaction.cpp
    ${CMAKE_SOURCE_DIR}/src/sha2.cpp
)

target_include_directories(cslp_fuzzcrash_slpparse PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(cslp_fuzzcrash_slpparse
    absl::variant
    absl::flat_hash_map
)

add_executable(cslp_fuzzcrash_bchparse
    ${CMAKE_CURRENT_SOURCE_DIR}/fuzzcrash_slpparse.cpp
    ${CMAKE_SOURCE_DIR}/src/slp_transaction.cpp
)

target_include_directories(cslp_fuzzcrash_bchparse PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(cslp_fuzzcrash_bchparse
    absl::variant
)

add_executable(cslp_differential_nodejs
    ${CMAKE_CURRENT_SOURCE_DIR}/differential_nodejs.cpp
    ${CMAKE_SOURCE_DIR}/src/slp_transaction.cpp
    ${CMAKE_SOURCE_DIR}/src/sha2.cpp
)

target_include_directories(cslp_differential_nodejs PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_BINARY_DIR}/nlohmann-json/include
    ${CMAKE_BINARY_DIR}/cpp-httplib/include
)

target_link_libraries(cslp_differential_nodejs
    absl::variant
    absl::flat_hash_map
    absl::numeric
    absl::strings
    httplib
    ${Boost_FILESYSTEM_LIBRARY}
)

add_executable(cslp_differential_python
    ${CMAKE_CURRENT_SOURCE_DIR}/differential_python.cpp
    ${CMAKE_SOURCE_DIR}/src/slp_transaction.cpp
    ${CMAKE_SOURCE_DIR}/src/sha2.cpp
)

target_include_directories(cslp_differential_python PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_BINARY_DIR}/nlohmann-json/include
    ${CMAKE_BINARY_DIR}/cpp-httplib/include
)

target_link_libraries(cslp_differential_python
    absl::variant
    absl::flat_hash_map
    absl::numeric
    absl::strings
    httplib
    ${Boost_FILESYSTEM_LIBRARY}
)


add_executable(cslp_differential_bitcoinrpc
    ${CMAKE_CURRENT_SOURCE_DIR}/differential_bitcoinrpc.cpp
    ${CMAKE_SOURCE_DIR}/src/slp_transaction.cpp
    ${CMAKE_SOURCE_DIR}/src/sha2.cpp
    ${CMAKE_SOURCE_DIR}/src/rpc.cpp
)

target_include_directories(cslp_differential_bitcoinrpc PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_BINARY_DIR}/nlohmann-json/include
    ${CMAKE_BINARY_DIR}/cpp-httplib/include
)

target_link_libraries(cslp_differential_bitcoinrpc
    absl::variant
    absl::flat_hash_map
    absl::numeric
    absl::strings
    httplib
    ${Boost_FILESYSTEM_LIBRARY}
)
