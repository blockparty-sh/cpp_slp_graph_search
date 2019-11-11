project(cslp_fuzzing)

add_executable(cslp_fuzzing
    ${CMAKE_CURRENT_SOURCE_DIR}/fuzzing.c
)

target_include_directories(cslp_fuzzing PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(cslp_fuzzing
    cslp
)

add_executable(cslp_differential
    ${CMAKE_CURRENT_SOURCE_DIR}/differential.cpp
    ${CMAKE_SOURCE_DIR}/src/slp_transaction.cpp
    ${CMAKE_SOURCE_DIR}/src/sha2.cpp
)

target_include_directories(cslp_differential PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_BINARY_DIR}/nlohmann-json/include
)

target_link_libraries(cslp_differential
    absl::flat_hash_map
    absl::numeric
    absl::strings
    ${Boost_FILESYSTEM_LIBRARY}
    ${Boost_PROCESS_LIBRARY}
)
