project(blockdecoder)

add_executable(blockdecoder
    ${CMAKE_CURRENT_SOURCE_DIR}/blockdecoder.cpp
    ${CMAKE_SOURCE_DIR}/src/transaction.cpp
    ${CMAKE_SOURCE_DIR}/src/slp_transaction.cpp
    ${CMAKE_SOURCE_DIR}/src/sha2.cpp
    ${CMAKE_SOURCE_DIR}/src/block.cpp
    ${CMAKE_SOURCE_DIR}/src/util.cpp
)

target_include_directories(blockdecoder PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(blockdecoder
    absl::variant
    absl::flat_hash_map
)
