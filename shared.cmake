# Shared logic between tlib's CMakelists and Renode's

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(DEBUG_DEFS "-DDEBUG -DDEBUG_ON")
endif()

option (HOST_BIG_ENDIAN "Host big endian" OFF)

if(NOT DEFINED HOST_ARCH)
    message(STATUS "'HOST_ARCH' isn't set; analyzing the CPU (${CMAKE_SYSTEM_PROCESSOR})...")
    if(${CMAKE_SYSTEM_PROCESSOR} MATCHES "(amd64|86)")
        set (HOST_ARCH "i386" CACHE STRING "Host architecture")
    elseif(${CMAKE_SYSTEM_PROCESSOR} MATCHES "(aarch64|arm64)")
        set (HOST_ARCH "aarch64" CACHE STRING "Host architecture")
    # Has to come last to not match arm macs arm64, while still matching a cpu like armv7l
    elseif(${CMAKE_SYSTEM_PROCESSOR} MATCHES "(arm)")
        set (HOST_ARCH "arm" CACHE STRING "Host architecture")
    else()
        message(FATAL_ERROR "CMAKE_SYSTEM_PROCESSOR '${CMAKE_SYSTEM_PROCESSOR}' doesn't seem to be supported. Supported host architectures are: 'arm', 'i386', 'aarch64/arm64'. Please set 'HOST_ARCH' manually.")
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

if (HOST_ARCH MATCHES "(arm|aarch64)" AND CMAKE_BUILD_TYPE STREQUAL "Release" AND NOT CMAKE_C_COMPILER_ID STREQUAL "AppleClang")
    # gcc emits a clobber error for unwind.h DECLARE_ENV_PTR() in release mode on arm targets
    # so demote it to a warning in this case, but not on arm mac clang, because this flag does not exsist
    add_definitions(-Wno-error=clobbered)
    add_definitions(-Wno-clobbered)
endif()

option (TARGET_BIG_ENDIAN "Target big endian" OFF)
set (TARGET_ARCH "" CACHE STRING "Target architecture")
set (TARGET_WORD_SIZE "32" CACHE STRING "Target word size")
message(STATUS "Target is: ${TARGET_WORD_SIZE}-bit ${TARGET_ARCH}")

set_property (CACHE HOST_ARCH PROPERTY STRINGS i386 arm aarch64)
set_property (CACHE TARGET_ARCH PROPERTY STRINGS i386 x86_64 arm arm-m arm64 sparc ppc ppc64 riscv riscv64 xtensa)

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

if("${TARGET_ACTUAL_ARCH}" STREQUAL "ppc64")
    set (TARGET_ACTUAL_ARCH "ppc")
endif()
if("${TARGET_ACTUAL_ARCH}" STREQUAL "riscv64")
    set (TARGET_ACTUAL_ARCH "riscv")
endif()
if("${TARGET_ACTUAL_ARCH}" STREQUAL "x86_64")
    set (TARGET_ACTUAL_ARCH "i386")
endif()

set(TARGET_INSN_START_EXTRA_WORDS 0)
if("${TARGET_ACTUAL_ARCH}" MATCHES "^(arm|i386|sparc)$")
    set(TARGET_INSN_START_EXTRA_WORDS 1)
elseif("${TARGET_ACTUAL_ARCH}" STREQUAL "arm64")
    set(TARGET_INSN_START_EXTRA_WORDS 2)
endif()

if("${TARGET_ACTUAL_ARCH}" STREQUAL "arm64" AND NOT "${TARGET_WORD_SIZE}" STREQUAL "64")
    message (FATAL_ERROR "ERROR: arm64 target has to be built with TARGET_WORD_SIZE=64")
endif()

string (TOUPPER "${HOST_ARCH}" HOST_ARCH_U)
string (TOUPPER "${TARGET_ACTUAL_ARCH}" TARGET_ACTUAL_ARCH_U)

add_definitions (
    -fPIC
    -Wall
    -Wextra
    -Wno-unused-parameter
    -Wno-sign-compare

    -DHOST_BITS_${HOST_WORD_SIZE}
    -DHOST_${HOST_ARCH_U}=1
    -DHOST_LONG_BITS=${HOST_WORD_SIZE}

    -DTARGET_${TARGET_ACTUAL_ARCH_U}=1

    -DTARGET_SHORT_ALIGNMENT=2
    -DTARGET_INT_ALIGNMENT=4
    -DTARGET_LONG_ALIGNMENT=4
    -DTARGET_LLONG_ALIGNMENT=4

    -DTARGET_LONG_BITS=${TARGET_WORD_SIZE}
    -DTARGET_INSN_START_EXTRA_WORDS=${TARGET_INSN_START_EXTRA_WORDS}
    ${ARM_M_DEF}
)
