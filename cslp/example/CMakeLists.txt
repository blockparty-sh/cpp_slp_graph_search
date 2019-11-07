project(cslp_example)

add_executable(cslp_example
    ${CMAKE_CURRENT_SOURCE_DIR}/example.c
)

target_include_directories(cslp_example PRIVATE
    ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(cslp_example
    cslp
)
