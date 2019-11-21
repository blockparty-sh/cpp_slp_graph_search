add_executable(unit-test
    ${CMAKE_CURRENT_SOURCE_DIR}/test.cpp
    ${CMAKE_SOURCE_DIR}/src/utxodb.cpp
    ${CMAKE_SOURCE_DIR}/src/bch.cpp
    ${CMAKE_SOURCE_DIR}/src/util.cpp
    ${CMAKE_SOURCE_DIR}/src/transaction.cpp
    ${CMAKE_SOURCE_DIR}/src/slp_transaction.cpp
    ${CMAKE_SOURCE_DIR}/src/sha2.cpp
    ${CMAKE_SOURCE_DIR}/src/slp_validator.cpp
)

target_include_directories(unit-test PUBLIC
    ${CMAKE_SOURCE_DIR}/include
    ${CATCH_INCLUDE_DIR}
    ${CMAKE_BINARY_DIR}/cpp-httplib/include
    ${CMAKE_BINARY_DIR}/nlohmann-json/include
)

target_link_libraries(unit-test
    Catch2::Catch2
    absl::flat_hash_map
    absl::flat_hash_set
    absl::node_hash_map
    base64
    spdlog
    ${Boost_THREAD_LIBRARY}
    ${Boost_SYSTEM_LIBRARY}
)
