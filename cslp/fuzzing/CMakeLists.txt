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
