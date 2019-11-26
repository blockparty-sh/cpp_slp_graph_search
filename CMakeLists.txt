cmake_minimum_required(VERSION 3.9)
project(gs++ VERSION 1 LANGUAGES CXX)

configure_file(include/config.h.in include/config.h ESCAPE_QUOTES)


set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
SET(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
SET(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)


option(${PROJECT_NAME}_BUILD_SERVER "Build ${PROJECT_NAME} server" ON)
option(${PROJECT_NAME}_BUILD_CLI "Build ${PROJECT_NAME} cli" ON)
option(${PROJECT_NAME}_BUILD_TXDECODER "Build slp transaction decoder utility" ON)
option(${PROJECT_NAME}_BUILD_BLOCKDECODER "Build block decoder utility" ON)
option(${PROJECT_NAME}_BUILD_TESTS "Build ${PROJECT_NAME} tests" ON)
option(${PROJECT_NAME}_BUILD_CSLP "Build ${PROJECT_NAME} cslp library" OFF)
option(${PROJECT_NAME}_BUILD_FUZZ "Build ${PROJECT_NAME} fuzzing programs" OFF)
option(${PROJECT_NAME}_SUPERBUILD "Build ${PROJECT_NAME} and the projects it depends on." ON)

# TODO should these be here
set(JSON_BuildTests OFF CACHE INTERNAL "")
set(BUILD_TESTING OFF)
set(CPPZMQ_BUILD_TESTS OFF)
set(toml11_BUILD_TEST OFF)
set(BASE64_BUILD_TESTS OFF)
set(JSON_BuildTests OFF)

# TODO does this actually work
include(CheckCXXCompilerFlag)
CHECK_CXX_COMPILER_FLAG("-march=native" COMPILER_SUPPORTS_MARCH_NATIVE)
if(COMPILER_SUPPORTS_MARCH_NATIVE)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -march=native -mno-tbm")
endif()


list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake")

find_package(Boost COMPONENTS system filesystem thread REQUIRED)
find_package(Protobuf REQUIRED)
find_package(GRPC REQUIRED)
find_package(Threads REQUIRED)



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

    ExternalProject_Add(slp-unit-test-data
        PREFIX slp-unit-test-data
        GIT_REPOSITORY "https://github.com/simpleledger/slp-unit-test-data.git"
        GIT_TAG "b8f55f7d0c93ce2a51eea1eab3f8f201ca0f6187"
        INSTALL_DIR     "${CMAKE_BINARY_DIR}/slp-unit-test-data"
        CONFIGURE_COMMAND ""
        BUILD_COMMAND ""
        INSTALL_COMMAND ""
        CMAKE_CACHE_ARGS
            -DCMAKE_INSTALL_PREFIX:PATH=${CMAKE_CURRENT_BINARY_DIR}/slp-unit-test-data
    )
    set(slp-unit-test-data_DIR "${CMAKE_CURRENT_BINARY_DIR}/slp-unit-test-data")

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
        GIT_TAG "5ee82617437e3090db568e9b6309fd38a64cc589"
        INSTALL_DIR     "${CMAKE_BINARY_DIR}/cppzmq.git"
        CMAKE_CACHE_ARGS
            -DCMAKE_INSTALL_PREFIX:PATH=${CMAKE_CURRENT_BINARY_DIR}/cppzmq
            -DCPPZMQ_BUILD_TESTS:BOOL=OFF
            -DZMQ_BUILD_TESTS:BOOL=OFF
    )
    set(cppzmq_DIR "${CMAKE_CURRENT_BINARY_DIR}/cppzmq/src/cppzmq")

    ExternalProject_Add(nlohmann-json
        PREFIX nlohmann-json
        GIT_REPOSITORY "https://github.com/nlohmann/json.git"
        GIT_TAG "v3.7.0"
        INSTALL_DIR     "${CMAKE_BINARY_DIR}/nlohmann-json"
        CMAKE_CACHE_ARGS
            -DCMAKE_INSTALL_PREFIX:PATH=${CMAKE_CURRENT_BINARY_DIR}/nlohmann-json
    )
    set(nlohmann-json_DIR "${CMAKE_CURRENT_BINARY_DIR}/nlohmann-json/src/nlohmann-json")

    ExternalProject_Add(toml11
        PREFIX toml11
        GIT_REPOSITORY "https://github.com/ToruNiina/toml11.git"
        GIT_TAG "26a09b2f65a3ee814abfab4db1c3f831a9282c5b"
        INSTALL_DIR     "${CMAKE_BINARY_DIR}/toml11"
        CMAKE_CACHE_ARGS
            -DCMAKE_INSTALL_PREFIX:PATH=${CMAKE_CURRENT_BINARY_DIR}/toml11
            -Dtoml11_BUILD_TEST:BOOL=OFF
    )
    set(toml11_DIR "${CMAKE_CURRENT_BINARY_DIR}/toml11/src/toml11")


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
            -Dslp-unit-test-data_DIR:PATH=${slp-unit-test-data_DIR}
            -Dabsl_DIR:PATH=${absl_DIR}
            -Dbase64_DIR:PATH=${base64_DIR}
            -Dcatch_DIR:PATH=${catch_DIR}
            -DCATCH_INCLUDE_DIR:PATH=${CATCH_INCLUDE_DIR}
            -Dspdlog_DIR:PATH=${spdlog_DIR}
            -Dcpp-httplib_DIR:PATH=${cpp-httplib_DIR}
            -Dcppzmq_DIR:PATH=${cppzmq_DIR}
            -Dnlohmann-json_DIR:PATH=${nlohmann-json_DIR}
            -Dtoml11_DIR:PATH=${toml11_DIR}
            -Dtoml11_BUILD_TEST:BOOL=OFF
            -DJSON_BuildTests:BOOL=OFF
            -DBUILD_TESTING:BOOL=OFF
            -DCPPZMQ_BUILD_TESTS:BOOL=OFF
            -Dtoml11_BUILD_TEST:BOOL=OFF
            -DBASE64_BUILD_TESTS:BOOL=OFF
            -DJSON_BuildTests:BOOL=OFF

        INSTALL_COMMAND ""
        DEPENDS
            slp-unit-test-data
            absl
            base64
            catch
            spdlog
            cpp-httplib
            cppzmq
            nlohmann-json
            toml11
    )
    ExternalProject_AlwaysConfigure(${PROJECT_NAME})
    return()
endif()

message(STATUS "Configuring inner-build")

add_subdirectory(${absl_DIR})
add_subdirectory(${base64_DIR})
add_subdirectory(${spdlog_DIR})
add_subdirectory(${cpp-httplib_DIR})
add_subdirectory(${cppzmq_DIR})
add_subdirectory(${nlohmann-json_DIR})
add_subdirectory(${toml11_DIR})


# TODO this code should be in pb directory probably
set (PROTOS
    ${CMAKE_CURRENT_SOURCE_DIR}/pb/graphsearch.proto
    ${CMAKE_CURRENT_SOURCE_DIR}/pb/utxo.proto
)

set(PROTO_SRC_DIR ${CMAKE_CURRENT_BINARY_DIR}/proto-src)
file(MAKE_DIRECTORY ${PROTO_SRC_DIR})
include_directories(${PROTO_SRC_DIR})


if (${PROJECT_NAME}_BUILD_SERVER)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/server)
endif()

if (${PROJECT_NAME}_BUILD_CLI)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/cli)
endif()

if (${PROJECT_NAME}_BUILD_TXDECODER)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/txdecoder)
endif()

if (${PROJECT_NAME}_BUILD_BLOCKDECODER)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/blockdecoder)
endif()

if (${PROJECT_NAME}_BUILD_TESTS)
    add_subdirectory(${catch_DIR})
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/test)
endif()

if (${PROJECT_NAME}_BUILD_FUZZ)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/fuzz)
endif()

if (${PROJECT_NAME}_BUILD_CSLP)
    find_package(SWIG REQUIRED)
    find_package(PythonLibs)
    include(${SWIG_USE_FILE})
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/cslp)
endif()

