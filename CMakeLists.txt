cmake_minimum_required(VERSION 3.0)
include (ExternalProject)

project (tlib)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(DEBUG_DEFS "-DDEBUG -DDEBUG_ON")
endif()

option (HOST_BIG_ENDIAN "Host big endian" OFF)

if(NOT DEFINED HOST_ARCH)
    message(STATUS "'HOST_ARCH' isn't set; analyzing the CPU (${CMAKE_SYSTEM_PROCESSOR})...")
    if(${CMAKE_SYSTEM_PROCESSOR} MATCHES "(aarch64|arm)")
        set (HOST_ARCH "arm" CACHE STRING "Host architecture")
    elseif(${CMAKE_SYSTEM_PROCESSOR} MATCHES "(amd64|86)")
        set (HOST_ARCH "i386" CACHE STRING "Host architecture")
    else()
        message(FATAL_ERROR "CMAKE_SYSTEM_PROCESSOR '${CMAKE_SYSTEM_PROCESSOR}' doesn't seem to be supported. Supported host architectures are: 'arm', 'i386'. Please set 'HOST_ARCH' manually.")
    endif()
endif()
message(STATUS "Using HOST_ARCH: ${HOST_ARCH}")

# Detect whether the host is 32- or 64-bit
math (EXPR WORD_SIZE_FOUND "${CMAKE_SIZEOF_VOID_P} * 8")
set (HOST_WORD_SIZE "${WORD_SIZE_FOUND}" CACHE STRING "Host word size")
message(STATUS "Using HOST_WORD_SIZE: ${HOST_WORD_SIZE}")

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
	set(CMAKE_BUILD_TYPE "Release" CACHE
	      STRING "Choose the type of build." FORCE)
	set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS
	    "Debug" "Release" "RelWithDebInfo")
endif()

option (TLIB_PROFILING_BUILD "Build optimized for profiling" OFF)
if(TLIB_PROFILING_BUILD)
    message(STATUS "Profiling enhancements are enabled")
    add_definitions (
        # Forcing not to omit frame pointer can have negative impact on performance, 
        # so the end profiling result will not exactly be equal to running on live system.
        # Unfortunately without frame pointers perf might have problem with unwinding stack
        # and call traces in reports will become less readable if using frame pointers for stack unwinding.
        -fno-omit-frame-pointer
        -g3
        -DTLIB_PROFILING_BUILD=1
    )
elseif(CMAKE_BUILD_TYPE STREQUAL "Release")
        add_definitions(-fomit-frame-pointer)
endif()

option (TARGET_BIG_ENDIAN "Target big endian" OFF)
set (TARGET_ARCH "" CACHE STRING "Target architecture")
set (TARGET_WORD_SIZE "32" CACHE STRING "Target word size")
message(STATUS "Target is: ${TARGET_WORD_SIZE}-bit ${TARGET_ARCH}")

set_property (CACHE HOST_ARCH PROPERTY STRINGS i386 arm)
set_property (CACHE TARGET_ARCH PROPERTY STRINGS i386 arm arm-m arm64 sparc ppc riscv xtensa)

if(NOT HOST_ARCH)
    message (FATAL_ERROR "Host architecture not set")
endif()

if(NOT TARGET_ARCH)
    message (FATAL_ERROR "Target architecture not set")
endif()

if(TARGET_BIG_ENDIAN)
    set (BIG_ENDIAN_DEF -DTARGET_WORDS_BIGENDIAN=1)
endif()

# Let's make 'TARGET_ACTUAL_ARCH' a lowercase 'TARGET_ARCH'.
string (TOLOWER "${TARGET_ARCH}" TARGET_ACTUAL_ARCH)

if("${TARGET_ACTUAL_ARCH}" STREQUAL "arm-m")
    set (TARGET_ACTUAL_ARCH "arm")
    set (ARM_M_DEF -DTARGET_PROTO_ARM_M=1)
endif()

set(TARGET_INSN_START_EXTRA_WORDS 0)
if("${TARGET_ACTUAL_ARCH}" MATCHES "^(arm|i386|sparc)$")
    set(TARGET_INSN_START_EXTRA_WORDS 1)
elseif("${TARGET_ACTUAL_ARCH}" STREQUAL "arm64")
    set(TARGET_INSN_START_EXTRA_WORDS 2)
endif()

set(TLIB_COMMIT_SHA "" CACHE STRING "Current tlib commit SHA")
if(NOT TLIB_COMMIT_SHA)
    execute_process(COMMAND git rev-parse --short HEAD RESULT_VARIABLE GIT_RESULT OUTPUT_VARIABLE GIT_OUTPUT OUTPUT_STRIP_TRAILING_WHITESPACE WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
    if(NOT GIT_RESULT EQUAL 0)
        set(GIT_OUTPUT "undefined")
    endif()

    set(TLIB_COMMIT_SHA "${GIT_OUTPUT}" CACHE STRING "Current tlib commit SHA" FORCE)
endif()

if("${TARGET_ACTUAL_ARCH}" STREQUAL "arm64" AND NOT "${TARGET_WORD_SIZE}" STREQUAL "64")
    message (FATAL_ERROR "ERROR: arm64 target has to be built with TARGET_WORD_SIZE=64")
endif()

# this must be placed after setting default values for:
# TARGET_BIG_ENDIAN, HOST_WORD_SIZE, TARGET_WORD_SIZE, TARGET_INSN_START_EXTRA_WORDS
ExternalProject_Add (tcglib
    SOURCE_DIR ${CMAKE_SOURCE_DIR}/tcg
    PREFIX tcg
    CMAKE_ARGS
        -DBIG_ENDIAN:BOOL=${TARGET_BIG_ENDIAN}
        -DCMAKE_BUILD_TYPE:STRING=${CMAKE_BUILD_TYPE}
        -DHOST_ARCHITECTURE:STRING=${HOST_ARCH}
        -DHOST_LONG_BITS:INT=${HOST_WORD_SIZE}
        -DTARGET_INSN_START_EXTRA_WORDS:INT=${TARGET_INSN_START_EXTRA_WORDS}
        -DTARGET_LONG_BITS:INT=${TARGET_WORD_SIZE}
        -DTLIB_PROFILING_BUILD:BOOL=${TLIB_PROFILING_BUILD}
    INSTALL_COMMAND "")

string (TOUPPER "${HOST_ARCH}" HOST_ARCH_U)
string (TOUPPER "${TARGET_ACTUAL_ARCH}" TARGET_ACTUAL_ARCH_U)

add_definitions (
    -fPIC
    -Wall
    -Wextra
    -Wno-unused-parameter
    -Wno-sign-compare
    -Werror

    -DTCG_TARGET_${HOST_ARCH_U}

    -DHOST_BITS_${HOST_WORD_SIZE}
    -DHOST_${HOST_ARCH_U}=1
    -DHOST_LONG_BITS=${HOST_WORD_SIZE}

    -DTARGET_SHORT_ALIGNMENT=2
    -DTARGET_INT_ALIGNMENT=4
    -DTARGET_LONG_ALIGNMENT=4
    -DTARGET_LLONG_ALIGNMENT=4

    -DTARGET_${TARGET_ACTUAL_ARCH_U}=1
    -DTARGET_LONG_BITS=${TARGET_WORD_SIZE}
    -DTARGET_INSN_START_EXTRA_WORDS=${TARGET_INSN_START_EXTRA_WORDS}

    -DTLIB_COMMIT=${TLIB_COMMIT_SHA}

    ${ARM_M_DEF}
    ${BIG_ENDIAN_DEF}
    ${DEBUG_DEFS}
    )

if("${TARGET_ACTUAL_ARCH}" STREQUAL "arm" OR "${TARGET_ACTUAL_ARCH}" STREQUAL "arm64")
    file (GLOB SOURCES_ADDITIONAL "arch/arm_common/*.c")
    set (HEADERS_ADDITIONAL "arch/arm_common")
endif()

include_directories (
    tcg
    softfloat-2
    include
    include_empty
    tcg/${HOST_ARCH}
    arch/${TARGET_ACTUAL_ARCH}
    ${HEADERS_ADDITIONAL}
    )

file (GLOB SOURCES
    "*.c"
    "softfloat-2/*.c"
    "arch/*.c"
    "external/*.c"
    "arch/${TARGET_ACTUAL_ARCH}/*.c"
    ${SOURCES_ADDITIONAL}
    )

add_library (tlib SHARED ${SOURCES})

add_dependencies (tlib tcglib)

if("${TARGET_ACTUAL_ARCH}" STREQUAL "i386")
    set (MATH_LIB_LINK_ARG "-lm" CACHE STRING
      "Argument pointing linker to a math functions library. It's required to translate i386 code.")
endif()

# On x86_64 Linux, the memcpy function was modified in GNU libc v2.14.
# It'd be impossible to run tlib without this wrapping with older libc.
if(${CMAKE_HOST_SYSTEM_NAME} STREQUAL Linux AND ${HOST_ARCH} STREQUAL i386 AND ${HOST_WORD_SIZE} EQUAL 64)
    set(WRAP_MEMCPY_OPT -Wl,--wrap=memcpy)
endif()

target_link_libraries (tlib
    ${WRAP_MEMCPY_OPT}
    -fPIC
    $<$<C_COMPILER_ID:GNU>:-zdefs>  # Error for undefined symbols is a default for Clang

    ${MATH_LIB_LINK_ARG}
    pthread
    ${CMAKE_CURRENT_BINARY_DIR}/tcg/src/tcglib-build/libtcg.a
    )
