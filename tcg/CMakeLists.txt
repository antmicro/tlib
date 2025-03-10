cmake_minimum_required(VERSION 3.12)

project (tcg LANGUAGES C)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(DEBUG_DEF "-DDEBUG")
endif()

set (HOST_ARCH "" CACHE STRING "Host architecture")
set_property (CACHE HOST_ARCH PROPERTY STRINGS i386 arm)

if(NOT HOST_ARCH)
    message (FATAL_ERROR "Host architecture not set")
endif()
if(NOT HOST_WORD_SIZE)
    message (FATAL_ERROR "Host word size not set")
endif()

# Make sure host architecture is lower case,
# as later we are including folder with this name
string (TOLOWER ${HOST_ARCH} HOST_LOWER)

add_definitions (
    -fPIC

    -DTARGET_LONG_BITS=${TARGET_WORD_SIZE}
    -DHOST_LONG_BITS=${HOST_WORD_SIZE}
    -DTARGET_INSN_START_EXTRA_WORDS=${TARGET_INSN_START_EXTRA_WORDS}

    ${BIG_ENDIAN_DEF}
    ${DEBUG_DEF}
    )

include_directories (
    ${HOST_LOWER}
    ${CMAKE_SOURCE_DIR}
    )

add_library (tcg OBJECT
    additional.c
    host-utils.c
    optimize.c
    tcg.c
    tcg-op-gvec.c
    tcg-op-vec.c
    tcg-runtime.c
    tcg-runtime-gvec.c
    )

