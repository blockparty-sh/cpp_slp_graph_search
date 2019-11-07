add_library(cslp STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/cslp.cpp
    ${CMAKE_SOURCE_DIR}/src/util.cpp
    ${CMAKE_SOURCE_DIR}/src/slp_transaction.cpp
    ${CMAKE_SOURCE_DIR}/src/sha2.cpp
    ${CMAKE_SOURCE_DIR}/src/slp_validator.cpp
)

target_include_directories(cslp PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(cslp
    PRIVATE absl::flat_hash_map
    PRIVATE absl::node_hash_map
)

add_subdirectory(example)
# add_subdirectory(swig)
