cmake_minimum_required(VERSION 3.16)
project(B64 VERSION 2.0.0 LANGUAGES C CXX)

# GoogleTest 1.11 and Google Benchmark 1.6 require C++14.
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Conan's cmake_find_package generators write their module/config files to
# the build dir; put it on the search path (harmless without Conan).
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
    # MSVC equivalent of the GNU/Clang flags in the else branch.
    set(B64_STRICT_WARNINGS /W4 /WX)
    # Silence MSVC's fopen "unsafe" (C4996). -D required: the shim above
    # forwards add_compile_definitions to add_definitions.
    add_compile_definitions(-D_CRT_SECURE_NO_WARNINGS)
else()
    if (B64_PIC)
        add_compile_options(-fpic)
    endif()

    check_linker_flag(-Wl,--gc-sections GC_SECTIONS_FLAG)
    check_linker_flag(-Wl,-dead_strip DEADSTRIP_FLAG)
    # Section flags stay global; strict warnings go per-target so the
    # vendored submodules aren't built with -Werror -pedantic.
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

# SIMD bulk kernel: pick the one for the target (GNU/Clang x86 or arm64),
# else a scalar stub. MSVC and other compilers always get the stub.
string(TOLOWER "${CMAKE_SYSTEM_PROCESSOR}" B64_ARCH)
if (MSVC)
    set(B64_SIMD_SRC src/cencode_simd_none.c)
elseif (B64_ARCH MATCHES "x86_64|amd64|i[3-6]86")
    set(B64_SIMD_SRC src/cencode_simd_sse.c)
elseif (B64_ARCH MATCHES "aarch64|arm")
    # aarch64 always; ARMv7-A uses NEON only when built with it (the file
    # checks __ARM_NEON and otherwise falls back to scalar).
    set(B64_SIMD_SRC src/cencode_simd_neon.c)
else()
    set(B64_SIMD_SRC src/cencode_simd_none.c)
endif()

# libb64
add_library(
    b64
    ${B64_LIB_TYPE}
    include/b64/ccommon.h
    include/b64/cdecode.h
    include/b64/cencode.h
    include/b64/encode.h
    include/b64/decode.h
    src/b64_internal.h
    src/cencode.c
    src/cdecode.c
    ${B64_SIMD_SRC})
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

# Tests/benchmarks default on only when libb64 is the top-level project
# (so add_subdirectory() consumers don't inherit them).
if (CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    set(B64_IS_TOP_LEVEL TRUE)
else()
    set(B64_IS_TOP_LEVEL FALSE)
endif()
option(B64_BUILD_TESTS "Build libb64 tests and benchmarks" ${B64_IS_TOP_LEVEL})

if (B64_BUILD_TESTS)
    # Prefer Conan/system packages; fall back to third-party/ submodules.
    find_package(GTest QUIET)
    find_package(benchmark QUIET)

    # googletest/benchmark declare cmake_minimum_required < 3.5, which
    # CMake >= 4 rejects; relax the floor for those subdirectories.
    if (CMAKE_VERSION VERSION_GREATER_EQUAL 3.31
            AND NOT DEFINED CMAKE_POLICY_VERSION_MINIMUM)
        set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
    endif()

    # Treat vendored headers as SYSTEM so -Werror -pedantic ignores them
    # (find_package imported targets already do this).
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
        # googletest 1.11 doesn't export the namespaced targets; alias them.
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
        test/cpp_wrapper_tests.cpp
        test/simd_equivalence_tests.cpp)
    target_include_directories(b64-test PRIVATE test src)
    target_compile_options(b64-test PRIVATE ${B64_STRICT_WARNINGS})
    target_link_libraries(b64-test PRIVATE b64 GTest::gtest_main)
    include(GoogleTest)
    gtest_discover_tests(b64-test)

    # Conan exposes one benchmark::benchmark (bundles main); the submodule
    # splits out benchmark::benchmark_main. Link whichever has main().
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

