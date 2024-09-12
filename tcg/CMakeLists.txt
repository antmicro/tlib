cmake_minimum_required(VERSION 3.12)

project (tcg)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(DEBUG_DEF "-DDEBUG")
endif()

option (TLIB_PROFILING_BUILD "Build optimized for profiling" OFF)
option (BIG_ENDIAN "Big endian" OFF)
set (HOST_ARCHITECTURE "i386" CACHE STRING "Host architecture")
set_property (CACHE HOST_ARCHITECTURE PROPERTY STRINGS i386 arm)

# These are 'STRING' instead of 'INT' to prevent printing three such warnings:
# > CMake Warning (dev) at CMakeLists.txt:11 (set):
# >   implicitly converting 'INT' to 'STRING' type.
# > This warning is for project developers.  Use -Wno-dev to suppress it.
set (HOST_LONG_BITS 0 CACHE STRING "Host word size (0 - autodetection)")
set (TARGET_LONG_BITS 32 CACHE STRING "Target word size")
set (TARGET_INSN_START_EXTRA_WORDS 0 CACHE STRING "Number of extra words in insn_start needed by the target")

if(HOST_LONG_BITS EQUAL 0)
    # Determine bitness
    # 32 or 64 bits
    set (HOST_LONG_BITS 32 CACHE INT "Host word size (0 - autodetection)" FORCE)
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        set (HOST_LONG_BITS 64 CACHE INT "Host word size (0 - autodetection)" FORCE)
    endif()
endif()

# Make sure host architecture is lower case,
# as later we are including folder with this name
string (TOLOWER ${HOST_ARCHITECTURE} HOST_LOWER)

if(BIG_ENDIAN)
    set (BIG_ENDIAN_DEF -DTARGET_WORDS_BIGENDIAN)
endif()

if(TLIB_PROFILING_BUILD)
    add_definitions (
        # see main CMakeLists.txt for comment why we need this
        -fno-omit-frame-pointer
        -g3
        -DTLIB_PROFILING_BUILD=1
    )
elseif(CMAKE_BUILD_TYPE STREQUAL "Release")
    add_definitions (
        -fomit-frame-pointer
    )
endif()

add_definitions (
    -fPIC

    -DTARGET_LONG_BITS=${TARGET_LONG_BITS}
    -DHOST_LONG_BITS=${HOST_LONG_BITS}
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

