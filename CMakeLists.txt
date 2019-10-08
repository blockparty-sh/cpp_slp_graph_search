project(gs++)

add_definitions(-DGS_VERSION=0.1)

cmake_minimum_required(VERSION 3.9)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# test that filesystem header actually is there and works
try_compile(HAS_FS "${CMAKE_BINARY_DIR}/temp"
"${CMAKE_SOURCE_DIR}/test/has_filesystem.cpp"
            CMAKE_FLAGS -DCMAKE_CXX_STANDARD=17 -DCMAKE_CXX_STANDARD_REQUIRED=ON
            LINK_LIBRARIES stdc++fs)
if(HAS_FS)
    message(STATUS "Compiler has filesystem support")
else()
    message(FATAL_ERROR "Compiler is missing filesystem capabilities")
endif(HAS_FS)


set(JSON_BuildTests OFF CACHE INTERNAL "")
set(BUILD_TESTING OFF)

include(CheckCXXCompilerFlag)
CHECK_CXX_COMPILER_FLAG("-march=native" COMPILER_SUPPORTS_MARCH_NATIVE)
if(COMPILER_SUPPORTS_MARCH_NATIVE)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -march=native -mno-tbm")
endif()



list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake")

find_package(Protobuf REQUIRED)
find_package(GRPC REQUIRED)
find_package(Threads REQUIRED)


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


    ExternalProject_Add(absl
        PREFIX absl
        GIT_REPOSITORY "https://github.com/abseil/abseil-cpp.git"
        GIT_TAG "20190808"
        INSTALL_DIR     "${CMAKE_BINARY_DIR}/absl"
        CMAKE_CACHE_ARGS
            -DCMAKE_INSTALL_PREFIX:PATH=${CMAKE_CURRENT_BINARY_DIR}/absl
    )
    set(absl_DIR "${CMAKE_CURRENT_BINARY_DIR}/absl/src/absl")

    ExternalProject_Add(base64
        PREFIX base64
        GIT_REPOSITORY "https://github.com/BurningEnlightenment/base64-cmake.git"
        GIT_TAG "d20517370228473c72cdf3fe51f299e628f1d538"
        INSTALL_DIR     "${CMAKE_BINARY_DIR}/base64"
        CMAKE_CACHE_ARGS
            -DCMAKE_INSTALL_PREFIX:PATH=${CMAKE_CURRENT_BINARY_DIR}/base64
            -BASE64_WITH_AVX2:BOOL=OFF
            -BASE64_WITH_AVX:BOOL=OFF
            -BASE64_WITH_SSE41:BOOL=OFF
            -BASE64_WITH_SSE42:BOOL=OFF
    )
    set(base64_DIR "${CMAKE_CURRENT_BINARY_DIR}/base64/src/base64")

    ExternalProject_Add(
        catch
        PREFIX ${CMAKE_BINARY_DIR}/catch
        GIT_REPOSITORY "https://github.com/philsquared/Catch.git"
        GIT_TAG "v2.9.2"
        INSTALL_DIR     "${CMAKE_BINARY_DIR}/catch2"
        CMAKE_CACHE_ARGS
            -DCMAKE_INSTALL_PREFIX:PATH=${CMAKE_CURRENT_BINARY_DIR}/catch2
    )
    set(catch_DIR "${CMAKE_CURRENT_BINARY_DIR}/catch/src/catch")
    ExternalProject_Get_Property(catch source_dir)
    set(CATCH_INCLUDE_DIR ${source_dir}/include CACHE INTERNAL "Path to include folder for Catch")

    ExternalProject_Add(spdlog
        PREFIX spdlog
        GIT_REPOSITORY "https://github.com/gabime/spdlog.git"
        GIT_TAG "v1.3.1"
        INSTALL_DIR     "${CMAKE_BINARY_DIR}/spdlog"
        CMAKE_CACHE_ARGS
            -DCMAKE_INSTALL_PREFIX:PATH=${CMAKE_CURRENT_BINARY_DIR}/spdlog
            -DBUILD_TESTING:BOOL=OFF
            -DSPDLOG_BUILD_EXAMPLES:BOOL=OFF
            -DSPDLOG_BUILD_BENCH:BOOL=OFF
    )
    set(spdlog_DIR "${CMAKE_CURRENT_BINARY_DIR}/spdlog/src/spdlog")

    ExternalProject_Add(mio
        PREFIX mio
        GIT_REPOSITORY "https://github.com/mandreyel/mio"
        GIT_TAG "76251b8dde16bdac44acf2547be2470fd75703e1"
        INSTALL_DIR     "${CMAKE_BINARY_DIR}/mio"
        CMAKE_CACHE_ARGS
            -DCMAKE_INSTALL_PREFIX:PATH=${CMAKE_CURRENT_BINARY_DIR}/mio
    )
    set(mio_DIR "${CMAKE_CURRENT_BINARY_DIR}/mio/src/mio")

    ExternalProject_Add(cpp-httplib
        PREFIX cpp-httplib
        GIT_REPOSITORY "https://github.com/yhirose/cpp-httplib.git"
        GIT_TAG "v0.2.4"
        INSTALL_DIR     "${CMAKE_BINARY_DIR}/cpp-httplib"
        CMAKE_CACHE_ARGS
            -DCMAKE_INSTALL_PREFIX:PATH=${CMAKE_CURRENT_BINARY_DIR}/cpp-httplib
    )
    set(cpp-httplib_DIR "${CMAKE_CURRENT_BINARY_DIR}/cpp-httplib/src/cpp-httplib")

    ExternalProject_Add(cppzmq
        PREFIX cppzmq
        GIT_REPOSITORY "https://github.com/zeromq/cppzmq.git"
        GIT_TAG "v4.4.1"
        INSTALL_DIR     "${CMAKE_BINARY_DIR}/cppzmq.git"
        CMAKE_CACHE_ARGS
            -DCMAKE_INSTALL_PREFIX:PATH=${CMAKE_CURRENT_BINARY_DIR}/cppzmq
            -DCPPZMQ_BUILD_TESTS:BOOL=OFF
    )
    set(cppzmq_DIR "${CMAKE_CURRENT_BINARY_DIR}/cppzmq/src/cppzmq")

    ExternalProject_Add(nlohmann-json
        PREFIX nlohmann-json
        GIT_REPOSITORY "https://github.com/nlohmann/json.git"
        GIT_TAG "v3.7.0"
        INSTALL_DIR     "${CMAKE_BINARY_DIR}/nlohmann-json.git"
        CMAKE_CACHE_ARGS
            -DCMAKE_INSTALL_PREFIX:PATH=${CMAKE_CURRENT_BINARY_DIR}/nlohmann-json
    )
    set(nlohmann-json_DIR "${CMAKE_CURRENT_BINARY_DIR}/nlohmann-json/src/nlohmann-json")

    ExternalProject_Add(libmongoc
        GIT_REPOSITORY  "https://github.com/mongodb/mongo-c-driver.git"
        GIT_TAG         "1.12.0"
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
            -Dabsl_DIR:PATH=${absl_DIR}
            -Dbase64_DIR:PATH=${base64_DIR}
            -Dcatch_DIR:PATH=${catch_DIR}
            -DCATCH_INCLUDE_DIR:PATH=${CATCH_INCLUDE_DIR}
            -Dspdlog_DIR:PATH=${spdlog_DIR}
            -Dmio_DIR:PATH=${mio_DIR}
            -Dcpp-httplib_DIR:PATH=${cpp-httplib_DIR}
            -Dcppzmq_DIR:PATH=${cppzmq_DIR}
            -Dnlohmann-json_DIR:PATH=${nlohmann-json_DIR}
        INSTALL_COMMAND ""
        DEPENDS
            libmongocxx
            absl
            base64
            catch
            spdlog
            mio
            cpp-httplib
            cppzmq
            nlohmann-json
    )
    ExternalProject_AlwaysConfigure(${PROJECT_NAME})
    return()
endif()

message(STATUS "Configuring inner-build")

find_package(libmongocxx REQUIRED)
add_subdirectory(${absl_DIR})
add_subdirectory(${base64_DIR})
add_subdirectory(${catch_DIR})
add_subdirectory(${spdlog_DIR})
add_subdirectory(${mio_DIR})
add_subdirectory(${cpp-httplib_DIR})
add_subdirectory(${cppzmq_DIR})
add_subdirectory(${nlohmann-json_DIR})
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CATCH_INCLUDE_DIR}
    ${CMAKE_CURRENT_BINARY_DIR}/cpp-httplib/include
    ${CMAKE_CURRENT_BINARY_DIR}/nlohmann-json/include
)

set (PROTOS
    ${CMAKE_CURRENT_SOURCE_DIR}/pb/graphsearch.proto
)

set(PROTO_SRC_DIR ${CMAKE_CURRENT_BINARY_DIR}/proto-src)
file(MAKE_DIRECTORY ${PROTO_SRC_DIR})
include_directories(${PROTO_SRC_DIR})

protobuf_generate_cpp(PROTO_SRCS PROTO_HDRS ${PROTO_SRC_DIR} ${PROTOS})
grpc_generate_cpp(GRPC_SRCS GRPC_HDRS ${PROTO_SRC_DIR} ${PROTOS})


add_executable(gs++
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gs++.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/mdatabase.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/txgraph.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/bch.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/utxodb.cpp
    ${PROTO_SRCS}
    ${GRPC_SRCS}
)

target_link_libraries(gs++
    absl::flat_hash_map
    absl::node_hash_map
    base64
    gRPC::grpc++_reflection
    protobuf::libprotobuf
    ${LIBMONGOCXX_LIBRARIES}
    ${CMAKE_THREAD_LIBS_INIT}
    spdlog
    mio
    httplib
    cppzmq
    stdc++fs
)
target_include_directories(gs++ PUBLIC
    ${LIBMONGOCXX_INCLUDE_DIRS}
    ${CMAKE_CURRENT_BINARY_DIR}/mio/include
    ${CMAKE_CURRENT_BINARY_DIR}/cpp-httplib/include
)
target_compile_definitions(gs++ PUBLIC
    ${LIBMONGOCXX_DEFINITIONS}
)
target_compile_options(gs++ PRIVATE -Wall -pedantic)


add_executable(gs++-cli
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gs++-cli.cpp
    ${PROTO_SRCS}
    ${GRPC_SRCS}
)

target_link_libraries(gs++-cli
    gRPC::grpc++_reflection
    protobuf::libprotobuf
    absl::flat_hash_map
    absl::node_hash_map
    base64
)
target_compile_options(gs++-cli PRIVATE -Wall -pedantic)



add_executable(internals_test
    ${CMAKE_CURRENT_SOURCE_DIR}/test/internals_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/utxodb.cpp
    ${PROTO_SRCS}
    ${PROTO_SRCS}
    ${GRPC_SRCS}
)
target_include_directories(internals_test PUBLIC
    ${CATCH_INCLUDE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(internals_test
    Catch2::Catch2
    absl::flat_hash_map
    absl::node_hash_map
    gRPC::grpc++_reflection
    protobuf::libprotobuf
    base64
    mio
    spdlog
)
