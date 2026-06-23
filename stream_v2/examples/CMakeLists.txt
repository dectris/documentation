cmake_minimum_required(VERSION 3.28)
project(stream2_example)

if(NOT WIN32)
    option(BUILD_LIBZMQ "Build ZeroMQ library from source" OFF)
else()
    option(BUILD_LIBZMQ "Build ZeroMQ library from source" ON)
endif()

if(BUILD_LIBZMQ)
    # libzmq v4.3.4 does not support CMake 4
    set(CMAKE_POLICY_VERSION_MINIMUM 3.10)
    include(FetchContent)
    set(BUILD_SHARED OFF CACHE INTERNAL "")
    set(BUILD_STATIC ON CACHE INTERNAL "")
    set(BUILD_TESTS OFF CACHE BOOL "")
    set(ENABLE_CURVE OFF CACHE BOOL "")
    set(ENABLE_DRAFTS OFF CACHE BOOL "")
    FetchContent_Declare(
        libzmq
        GIT_REPOSITORY https://github.com/zeromq/libzmq.git
        GIT_TAG 4097855ddaaa65ed7b5e8cb86d143842a594eebd # v4.3.4
        EXCLUDE_FROM_ALL
    )
    FetchContent_MakeAvailable(libzmq)
    unset(ZEROMQ_LIBRARY CACHE)
    set(LIBZMQ_TARGET libzmq-static)
else()
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(PC_LIBZMQ QUIET libzmq)
    find_library(ZeroMQ_LIBRARY
        NAMES libzmq.so libzmq.dylib libzmq.dll
        PATHS ${PC_LIBZMQ_LIBDIR} ${PC_LIBZMQ_LIBRARY_DIRS}
        REQUIRED
        )
    add_library(libzmq SHARED IMPORTED)
    set_property(TARGET libzmq PROPERTY INTERFACE_INCLUDE_DIRECTORIES ${PC_LIBZMQ_INCLUDE_DIRS})
    set_property(TARGET libzmq PROPERTY IMPORTED_LOCATION ${ZeroMQ_LIBRARY})
    set(LIBZMQ_TARGET libzmq)
endif()

add_subdirectory(third_party EXCLUDE_FROM_ALL)

set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

include_directories(
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party"
    )

if(NOT "${CMAKE_C_COMPILER_ID}" MATCHES "MSVC")
    include(CheckCCompilerFlag)
    check_c_compiler_flag(-mf16c COMPILER_SUPPORTS_MF16C)
    if(COMPILER_SUPPORTS_MF16C)
        add_compile_options(-mf16c)
    endif()
    add_compile_options(-Wall -Wextra)
endif()

add_library(stream2 STATIC
    stream2.c
    stream2.h
    )
target_link_libraries(stream2 PRIVATE
    tinycbor
    )

add_executable(example example.c)
target_link_libraries(example
    ${LIBZMQ_TARGET}
    compression
    stream2
    tinycbor
    )
