project(txdecoder)

add_executable(txdecoder
    ${CMAKE_CURRENT_SOURCE_DIR}/txdecoder.cpp
    ${CMAKE_SOURCE_DIR}/src/slp_transaction.cpp
    ${CMAKE_SOURCE_DIR}/src/sha2.cpp
)

target_include_directories(txdecoder PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(txdecoder
    absl::variant
)
