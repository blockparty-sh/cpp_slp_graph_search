project(cslp_fuzzing)

add_executable(cslp_fuzzcrash
    ${CMAKE_CURRENT_SOURCE_DIR}/fuzzcrash.c
)

target_include_directories(cslp_fuzzcrash PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(cslp_fuzzcrash
    cslp
)

add_executable(cslp_differential_nodejs
    ${CMAKE_CURRENT_SOURCE_DIR}/differential_nodejs.cpp
    ${CMAKE_SOURCE_DIR}/src/slp_transaction.cpp
    ${CMAKE_SOURCE_DIR}/src/sha2.cpp
)

target_include_directories(cslp_differential_nodejs PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_BINARY_DIR}/nlohmann-json/include
)

target_link_libraries(cslp_differential_nodejs
    absl::flat_hash_map
    absl::numeric
    absl::strings
    ${Boost_FILESYSTEM_LIBRARY}
    ${Boost_PROCESS_LIBRARY}
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
    absl::flat_hash_map
    absl::numeric
    absl::strings
    mio
    httplib
    ${Boost_FILESYSTEM_LIBRARY}
)
