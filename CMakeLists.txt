cmake_minimum_required(VERSION 3.16)
project(B64 VERSION 2.0.0 LANGUAGES C CXX)

# GoogleTest 1.11 and Google Benchmark 1.6 require C++14.
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Make Conan-provided packages discoverable. The Conan 1.x
# cmake_find_package / cmake_find_package_multi generators write their
# Find*.cmake / *Config.cmake files into the folder where `conan install`
# runs (conventionally the CMake build dir), so put it on both search
# paths up front. Harmless when Conan is not used.
list(APPEND CMAKE_MODULE_PATH ${CMAKE_BINARY_DIR})
list(APPEND CMAKE_PREFIX_PATH ${CMAKE_BINARY_DIR})

include(GNUInstallDirs)
include(CheckCSourceCompiles)

option(B64_SHARED      "Enable shared library"            OFF)
option(B64_PIC         "Enable Position independent code" OFF)

if (NOT add_compile_definitions)
    macro(add_compile_definitions)
        add_definitions(${ARGV})
    endmacro()
endif()

if (NOT add_link_options)
    macro(add_link_options)
        set(CMAKE_C_LINK_FLAGS "${CMAKE_C_LINK_FLAGS} ${ARGV}")
        set(CMAKE_CXX_LINK_FLAGS "${CMAKE_CXX_LINK_FLAGS} ${ARGV}")
    endmacro()
endif()

macro(check_linker_flag FLAG VAR)
    message("Performing Test ${VAR}")
    unset(${VAR})
    file(
      WRITE
        ${CMAKE_CURRENT_BINARY_DIR}/stub.cpp
        "int main() { return 0; }\n")
    try_compile(
      ${VAR}
      ${CMAKE_CURRENT_BINARY_DIR}
      ${CMAKE_CURRENT_BINARY_DIR}/stub.cpp
      CMAKE_FLAGS -DCOMPILE_DEFINITIONS:STRING=${FLAG}
      OUTPUT_VARIABLE COMPILE_OUTPUT)
    if("${COMPILE_OUTPUT}" MATCHES "warning: cannot find entry symbol.*")
      set(${VAR} 0)
    endif()
    if (${VAR})
        message("Performing Test ${VAR} - Success")
    else()
        message("Performing Test ${VAR} - Failed")
    endif()
endmacro(check_linker_flag)

if (B64_SHARED)
   set(B64_LIB_TYPE SHARED)
else()
    set(B64_LIB_TYPE STATIC)
endif()

if (MSVC)
    # The GNU/Clang flags below do not apply to MSVC; use its equivalents.
    # /W4 /WX = high warning level, warnings-as-errors (applied per target
    # via ${B64_STRICT_WARNINGS}). Optimization is left to CMake's defaults.
    set(B64_STRICT_WARNINGS /W4 /WX)
else()
    if (B64_PIC)
        add_compile_options(-fpic)
    endif()

    check_linker_flag(-Wl,--gc-sections GC_SECTIONS_FLAG)
    check_linker_flag(-Wl,-dead_strip DEADSTRIP_FLAG)
    # Section flags are global (needed for the gc-sections/dead-strip linker
    # options below). The strict warning flags are applied per-target via
    # ${B64_STRICT_WARNINGS} so the vendored googletest/benchmark submodules
    # (added with add_subdirectory) are not compiled with -Werror -pedantic.
    add_compile_options(-ffunction-sections -fdata-sections)
    set(B64_STRICT_WARNINGS -Wall -Werror -pedantic)
    if (${GC_SECTIONS_FLAG})
        add_link_options(-Wl,--gc-sections)
    elseif (${DEADSTRIP_FLAG})
        add_link_options(-Wl,-dead_strip)
    endif()

    if ("${CMAKE_BUILD_TYPE}" STREQUAL "Debug")
        add_compile_options(-fstack-protector-all -O0)
        add_definitions(-DBUILD_DEBUG)
    elseif ("${CMAKE_BUILD_TYPE}" STREQUAL "Profile")
        add_compile_options($<$<COMPILE_LANGUAGE:CXX>:-fstrict-enums>)
        add_compile_options(-fmerge-all-constants -O3 -pg -fno-inline)
        add_compile_definitions(-DBUILD_PROFILE -DNDEBUG)
    elseif ("${CMAKE_BUILD_TYPE}" STREQUAL "Release")
        add_compile_options($<$<COMPILE_LANGUAGE:CXX>:-fstrict-enums>)
        add_compile_options(
            -fmerge-all-constants
            -fomit-frame-pointer
            -O3
            -fno-stack-protector)
        add_compile_definitions(-DBUILD_RELEASE -DNDEBUG)
    endif()
endif()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# libb64
add_library(
    b64
    ${B64_LIB_TYPE}
    include/b64/ccommon.h
    include/b64/cdecode.h
    include/b64/cencode.h
    include/b64/decode.h
    include/b64/encode.h
    src/cencode.c
    src/cdecode.c)
target_include_directories(
    b64
    PUBLIC ./include)
target_compile_options(b64 PRIVATE ${B64_STRICT_WARNINGS})
install(
    FILES
      include/b64/ccommon.h
      include/b64/cdecode.h
      include/b64/cencode.h
      include/b64/decode.h
      include/b64/encode.h
    TYPE INCLUDE)
install(TARGETS b64 LIBRARY ARCHIVE)

# base64
add_executable(
    base64
    base64/base64.cc)
target_link_libraries(base64 b64)
target_compile_options(base64 PRIVATE ${B64_STRICT_WARNINGS})
install(TARGETS base64 RUNTIME)

# examples
add_executable(
    c-example1
    examples/c-example1.c)
target_link_libraries(c-example1 b64)
target_compile_options(c-example1 PRIVATE ${B64_STRICT_WARNINGS})
add_custom_target(test-c-example1
    COMMAND echo "*** Test c-example1 ***"
    COMMAND ${CMAKE_BINARY_DIR}/c-example1
    DEPENDS c-example1)

add_executable(
    c-example2
    examples/c-example2.c)
target_link_libraries(c-example2 b64)
target_compile_options(c-example2 PRIVATE ${B64_STRICT_WARNINGS})
add_custom_target(test-c-example2
    COMMAND echo "*** Test c-example2 ***"
    COMMAND ${CMAKE_BINARY_DIR}/c-example2 loremgibson.txt encoded.txt decoded.txt
    COMMAND diff -q loremgibson.txt decoded.txt
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/examples
    DEPENDS c-example2)
install(
    TARGETS c-example1 c-example2
    RUNTIME
      DESTINATION ${CMAKE_INSTALL_BINDIR}/examples)

# tests & benchmarks — built by default only when libb64 is the top-level
# project, so consumers that add_subdirectory() libb64 don't inherit the
# test/benchmark targets and their dependencies.
if (CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    set(B64_IS_TOP_LEVEL TRUE)
else()
    set(B64_IS_TOP_LEVEL FALSE)
endif()
option(B64_BUILD_TESTS "Build libb64 tests and benchmarks" ${B64_IS_TOP_LEVEL})

if (B64_BUILD_TESTS)
    # Prefer Conan/system-provided packages; fall back to the vendored
    # submodules under third-party/ when not found. (Conan's generated
    # modules are found via CMAKE_MODULE_PATH/CMAKE_PREFIX_PATH set near
    # the top of this file.)
    find_package(GTest QUIET)
    find_package(benchmark QUIET)

    # googletest 1.11 / benchmark 1.6.1 declare cmake_minimum_required
    # below 3.5, which CMake >= 4.0 rejects. Relax the floor for the
    # vendored subdirectories so the offline fallback still configures.
    if (CMAKE_VERSION VERSION_GREATER_EQUAL 3.31
            AND NOT DEFINED CMAKE_POLICY_VERSION_MINIMUM)
        set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
    endif()

    # Mark a vendored dependency's headers as SYSTEM so our -Werror
    # -pedantic test/benchmark code isn't broken by third-party headers
    # (the Conan/find_package imported targets already do this).
    function(b64_mark_system)
        foreach(_t ${ARGN})
            if (TARGET ${_t})
                get_target_property(_inc ${_t} INTERFACE_INCLUDE_DIRECTORIES)
                if (_inc)
                    set_target_properties(${_t} PROPERTIES
                        INTERFACE_SYSTEM_INCLUDE_DIRECTORIES "${_inc}")
                endif()
            endif()
        endforeach()
    endfunction()

    if (NOT GTest_FOUND)
        message(STATUS "GTest not found; using third-party/googletest")
        add_subdirectory(third-party/googletest EXCLUDE_FROM_ALL)
        # googletest 1.11 does not export the namespaced targets that
        # find_package(GTest) provides; alias them so consumers match.
        if (NOT TARGET GTest::gtest_main)
            add_library(GTest::gtest_main ALIAS gtest_main)
        endif()
        if (NOT TARGET GTest::gmock_main)
            add_library(GTest::gmock_main ALIAS gmock_main)
        endif()
        b64_mark_system(gtest gtest_main gmock gmock_main)
    endif()

    if (NOT benchmark_FOUND)
        message(STATUS "benchmark not found; using third-party/benchmark")
        set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
        add_subdirectory(third-party/benchmark EXCLUDE_FROM_ALL)
        if (NOT TARGET benchmark::benchmark)
            add_library(benchmark::benchmark ALIAS benchmark)
        endif()
        b64_mark_system(benchmark benchmark_main)
    endif()

    enable_testing()

    # unit tests (GoogleTest) — held to the same -Werror -pedantic bar
    add_executable(b64-test
        test/encode_decode_tests.cpp
        test/regression_tests.cpp
        test/rfc_compliance_tests.cpp
        test/cpp_wrapper_tests.cpp)
    target_include_directories(b64-test PRIVATE test)
    target_compile_options(b64-test PRIVATE ${B64_STRICT_WARNINGS})
    target_link_libraries(b64-test PRIVATE b64 GTest::gtest_main)
    include(GoogleTest)
    gtest_discover_tests(b64-test)

    # microbenchmarks (Google Benchmark). Conan's find_package exposes a
    # single benchmark::benchmark target that already bundles the default
    # main(); the submodule splits it into benchmark::benchmark_main. Link
    # whichever provides main() (the source does not define its own).
    if (TARGET benchmark::benchmark_main)
        set(B64_BENCHMARK_MAIN benchmark::benchmark_main)
    else()
        set(B64_BENCHMARK_MAIN benchmark::benchmark)
    endif()
    add_executable(b64-benchmark benchmark/main.cpp)
    target_compile_options(b64-benchmark PRIVATE ${B64_STRICT_WARNINGS})
    target_link_libraries(b64-benchmark PRIVATE b64 ${B64_BENCHMARK_MAIN})

    # data-file generator for ad-hoc benchmarking / feeding the base64 CLI
    add_executable(b64-gendata tools/gendata.c)
    target_compile_options(b64-gendata PRIVATE ${B64_STRICT_WARNINGS})
endif()

