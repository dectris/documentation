cmake_minimum_required(VERSION 3.11)
project(stream2_example)

if(NOT WIN32)
    option(BUILD_LIBZMQ "Build ZeroMQ library from source" OFF)
else()
    option(BUILD_LIBZMQ "Build ZeroMQ library from source" ON)
endif()

if(BUILD_LIBZMQ)
    include(FetchContent)
    FetchContent_Declare(
        libzmq
        GIT_REPOSITORY https://github.com/zeromq/libzmq.git
        GIT_TAG 4097855ddaaa65ed7b5e8cb86d143842a594eebd # v4.3.4
        )
    FetchContent_GetProperties(libzmq)
    if(NOT libzmq_POPULATED)
        FetchContent_Populate(libzmq)
        set(BUILD_SHARED OFF CACHE INTERNAL "")
        set(BUILD_STATIC ON CACHE INTERNAL "")
        set(ENABLE_CURVE OFF CACHE BOOL "")
        set(ENABLE_DRAFTS OFF CACHE BOOL "")
        add_subdirectory("${libzmq_SOURCE_DIR}" "${libzmq_BINARY_DIR}" EXCLUDE_FROM_ALL)
        unset(ZEROMQ_LIBRARY CACHE)
    endif()
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
    add_compile_options(-mf16c -Wall -Wextra)
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
