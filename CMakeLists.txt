project(gs++)

add_definitions(-DGS_VERSION=0.1)

cmake_minimum_required(VERSION 3.5)

set(CMAKE_CXX_STANDARD 17)

set(JSON_BuildTests OFF CACHE INTERNAL "")
set(BUILD_TESTING OFF)


list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake")

find_package(Protobuf REQUIRED)
find_package(GRPC REQUIRED)

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/third-party/abseil-cpp)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/third-party/base64)


option(${PROJECT_NAME}_SUPERBUILD "Build ${PROJECT_NAME} and the projects it depends on." ON)

if(${PROJECT_NAME}_SUPERBUILD)

    include(ExternalProject)

    set(common_cmake_cache_args
        -DCMAKE_CXX_COMPILER:PATH=${CMAKE_CXX_COMPILER}
    )
    if(NOT DEFINED CMAKE_CONFIGURATION_TYPES)
        list(APPEND common_cmake_cache_args
            -DCMAKE_BUILD_TYPE:STRING=${CMAKE_BUILD_TYPE}
        )
    endif()

    ExternalProject_Add(libmongoc
        GIT_REPOSITORY  "https://github.com/mongodb/mongo-c-driver.git"
        GIT_TAG         "1.12.0"
        GIT_PROGRESS    1
        GIT_SHALLOW     1
        SOURCE_DIR      "${CMAKE_BINARY_DIR}/libmongoc"
        BINARY_DIR      "${CMAKE_BINARY_DIR}/libmongoc-build"
        INSTALL_DIR     "${CMAKE_BINARY_DIR}/libmongoc-install"
        CMAKE_CACHE_ARGS
            ${common_cmake_cache_args}
            -DCMAKE_INSTALL_PREFIX:PATH=${CMAKE_BINARY_DIR}/libmongoc-install
            -DENABLE_TESTS:BOOL=OFF
            -DENABLE_STATIC:BOOL=OFF
            -DENABLE_EXAMPLES:BOOL=OFF
            -DENABLE_EXTRA_ALIGNMENT:BOOL=OFF
        #INSTALL_COMMAND ""
    )
    set(libmongoc-1.0_DIR "${CMAKE_BINARY_DIR}/libmongoc-install/lib/cmake/libmongoc-1.0/")
    set(libbson-1.0_DIR "${CMAKE_BINARY_DIR}/libmongoc-install/lib/cmake/libbson-1.0/")

    ExternalProject_Add(libmongocxx
        GIT_REPOSITORY  "https://github.com/mongodb/mongo-cxx-driver.git"
        GIT_TAG         "r3.3.1"
        GIT_PROGRESS    1
        GIT_SHALLOW     1
        SOURCE_DIR      "${CMAKE_BINARY_DIR}/libmongocxx"
        BINARY_DIR      "${CMAKE_BINARY_DIR}/libmongocxx-build"
        INSTALL_DIR     "${CMAKE_BINARY_DIR}/libmongocxx-install"
        CMAKE_CACHE_ARGS
            ${common_cmake_cache_args}
            -DCMAKE_INSTALL_PREFIX:PATH=${CMAKE_BINARY_DIR}/libmongocxx-install
            -DBUILD_SHARED_LIBS:BOOL=ON
            -DENABLE_TESTS:BOOL=OFF
            -DENABLE_EXAMPLES:BOOL=OFF
            -DBSONCXX_POLY_USE_BOOST:BOOL=OFF
            -DBSONCXX_POLY_USE_MNMLSTC:BOOL=ON
            -DBSONCXX_POLY_USE_STD:BOOL=OFF
            -Dlibmongoc-1.0_DIR:PATH=${libmongoc-1.0_DIR}
            -Dlibbson-1.0_DIR:PATH=${libbson-1.0_DIR}
        DEPENDS
            libmongoc
    )
    set(libmongocxx_DIR "${CMAKE_BINARY_DIR}/libmongocxx-install/lib/cmake/libmongocxx-3.3.1/")
    set(libbsoncxx_DIR "${CMAKE_BINARY_DIR}/libmongocxx-install//lib/cmake/libbsoncxx-3.3.1/")


    function(ExternalProject_AlwaysConfigure proj)
      _ep_get_step_stampfile(${proj} "configure" stampfile)
      ExternalProject_Add_Step(${proj} forceconfigure
        COMMAND ${CMAKE_COMMAND} -E remove ${stampfile}
        COMMENT "Forcing configure step for '${proj}'"
        DEPENDEES build
        ALWAYS 1
        )
    endfunction()

    ExternalProject_Add(${PROJECT_NAME}
        SOURCE_DIR "${CMAKE_SOURCE_DIR}"
        BINARY_DIR "${CMAKE_BINARY_DIR}"
        DOWNLOAD_COMMAND ""
        UPDATE_COMMAND ""
        CMAKE_CACHE_ARGS
            ${common_cmake_cache_args}
            -D${PROJECT_NAME}_SUPERBUILD:BOOL=OFF
            -Dlibbsoncxx_DIR:PATH=${libbsoncxx_DIR}
            -Dlibmongocxx_DIR:PATH=${libmongocxx_DIR}
        INSTALL_COMMAND ""
        DEPENDS
            libmongocxx
    )
    ExternalProject_AlwaysConfigure(${PROJECT_NAME})
    return()
endif()

message(STATUS "Configuring inner-build")

find_package(libmongocxx REQUIRED)


set (PROTOS
    ${CMAKE_CURRENT_SOURCE_DIR}/pb/helloworld.proto
)

set(PROTO_SRC_DIR ${CMAKE_CURRENT_BINARY_DIR}/proto-src)
file(MAKE_DIRECTORY ${PROTO_SRC_DIR})
include_directories(${PROTO_SRC_DIR})

protobuf_generate_cpp(PROTO_SRCS PROTO_HDRS ${PROTO_SRC_DIR} ${PROTOS})
grpc_generate_cpp(GRPC_SRCS GRPC_HDRS ${PROTO_SRC_DIR} ${PROTOS})


add_executable(gs++
    ${CMAKE_CURRENT_SOURCE_DIR}/gs++.cpp
    ${PROTO_SRCS}
    ${GRPC_SRCS}
)

target_link_libraries(gs++
    absl::flat_hash_map
    absl::node_hash_map
    gRPC::grpc++_reflection
    protobuf::libprotobuf
    ${LIBMONGOCXX_LIBRARIES}
    stdc++fs
)
target_include_directories(gs++ PUBLIC
    ${LIBMONGOCXX_INCLUDE_DIRS}
)
target_compile_definitions(gs++ PUBLIC
    ${LIBMONGOCXX_DEFINITIONS}
)

add_executable(gs++-cli
    ${CMAKE_CURRENT_SOURCE_DIR}/gs++-cli.cpp
    ${PROTO_SRCS}
    ${GRPC_SRCS}
)

target_link_libraries(gs++-cli
    gRPC::grpc++_reflection
    protobuf::libprotobuf
	base64
)
