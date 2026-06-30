cmake_minimum_required(VERSION 3.16)
project(B64 VERSION 1.2.2 LANGUAGES C CXX)

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

if (B64_PIC)
   add_compile_options(-fpic)
endif()

if (B64_SHARED)
   set(B64_LIB_TYPE SHARED)
else()
    set(B64_LIB_TYPE STATIC)
endif()

check_linker_flag(-Wl,--gc-sections GC_SECTIONS_FLAG)
check_linker_flag(-Wl,-dead_strip DEADSTRIP_FLAG)
add_compile_options(
    -Wall
    -Werror
    -pedantic
    -ffunction-sections
    -fdata-sections)
if (${GC_SECTIONS_FLAG})
    add_link_options(-Wl,--gc-sections)
elseif (${DEADSTRIP_FLAG})
    add_link_options(-Wl,-dead_strip)
endif()

if (${CMAKE_BUILD_TYPE} STREQUAL "Debug")
    add_compile_options(-fstack-protector-all -O0)
    add_definitions(-DBUILD_DEBUG)
elseif (${CMAKE_BUILD_TYPE} STREQUAL "Profile")
    add_compile_options($<$<COMPILE_LANGUAGE:CXX>:-fstrict-enums>)
    add_compile_options(-fmerge-all-constants -O3 -pg -fno-inline)
    add_compile_definitions(-DBUILD_PROFILE -DNDEBUG)
elseif (${CMAKE_BUILD_TYPE} STREQUAL "Release")
    add_compile_options($<$<COMPILE_LANGUAGE:CXX>:-fstrict-enums>)
    add_compile_options(
        -fmerge-all-constants
        -fomit-frame-pointer
        -O3
        -fno-stack-protector)
    add_compile_definitions(-DBUILD_RELEASE -DNDEBUG)
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
install(TARGETS base64 RUNTIME)

# examples
add_executable(
    c-example1
    examples/c-example1.c)
target_link_libraries(c-example1 b64)
add_custom_target(test-c-example1
    COMMAND echo "*** Test c-example1 ***"
    COMMAND ${CMAKE_BINARY_DIR}/c-example1
    DEPENDS c-example1)

add_executable(
    c-example2
    examples/c-example2.c)
target_link_libraries(c-example2 b64)
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

