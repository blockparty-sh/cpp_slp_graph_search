project(slpdecoder)

add_executable(slpdecoder
    ${CMAKE_CURRENT_SOURCE_DIR}/slpdecoder.cpp
    ${CMAKE_SOURCE_DIR}/src/slp_transaction.cpp
    ${CMAKE_SOURCE_DIR}/src/sha2.cpp
)

target_include_directories(slpdecoder PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(slpdecoder
    absl::variant
)
