project(interpreter)

add_executable(interpreter
    ${CMAKE_CURRENT_SOURCE_DIR}/interpreter.cpp
    ${CMAKE_SOURCE_DIR}/src/transaction.cpp
    ${CMAKE_SOURCE_DIR}/src/slp_transaction.cpp
    ${CMAKE_SOURCE_DIR}/src/sha2.cpp
)

target_include_directories(interpreter PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(interpreter
    absl::variant
    absl::flat_hash_map
    absl::node_hash_map
)
