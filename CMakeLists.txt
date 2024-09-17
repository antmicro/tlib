cmake_minimum_required(VERSION 3.12)
include (ExternalProject)

project (tlib)

include (shared.cmake)

set(TLIB_COMMIT_SHA "" CACHE STRING "Current tlib commit SHA")
if(NOT TLIB_COMMIT_SHA)
    execute_process(COMMAND git rev-parse --short HEAD RESULT_VARIABLE GIT_RESULT OUTPUT_VARIABLE GIT_OUTPUT OUTPUT_STRIP_TRAILING_WHITESPACE WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
    if(NOT GIT_RESULT EQUAL 0)
        set(GIT_OUTPUT "undefined")
    endif()

    set(TLIB_COMMIT_SHA "${GIT_OUTPUT}" CACHE STRING "Current tlib commit SHA" FORCE)
endif()

# tcg generates tons of warning so turn off some for now
remove_definitions(-Wall -Wextra -Wno-unused-parameter -Wno-sign-compare)
add_subdirectory(tcg)
add_definitions(-Wall -Wextra -Wno-unused-parameter -Wno-sign-compare)

set(SOFTFLOAT_SPECIALIZED_ARCHITECTURE "8086")
if("${TARGET_ACTUAL_ARCH}" STREQUAL "riscv" OR "${TARGET_ACTUAL_ARCH}" STREQUAL "riscv64")
    set(SOFTLOAT_SPECIALIZED_ARCHITECTURE "RISCV")
endif()
add_subdirectory (softfloat-3 EXCLUDE_FROM_ALL)

if (NOT CMAKE_C_COMPILER_ID MATCHES "(clang)" AND NOT "${TARGET_ACTUAL_ARCH}" STREQUAL "arm64")
    # Clang throws warnings from the arm64 target's function stubs
    # so do not enable -Werror in that case until that is fixed
    add_definitions(-Werror)
endif()


add_definitions (
    -DTCG_TARGET_${HOST_ARCH_U}
    -DTLIB_COMMIT=${TLIB_COMMIT_SHA}
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

set(TLIB_LIB_TYPE "SHARED" CACHE STRING "")
if ("${CMAKE_HOST_SYSTEM_NAME}" STREQUAL "Linux")
    # On non-Linux hosts the TLIB_LIB_TYPE variable here gets expanded to "" for some unkown reason
    # So until that is figured out we only support picking library type on linux
    add_library (tlib ${TLIB_LIB_TYPE} ${SOURCES} $<TARGET_OBJECTS:tcg> $<TARGET_OBJECTS:softfloat-3>)
else ()
    add_library (tlib OBJECT ${SOURCES} $<TARGET_OBJECTS:tcg> $<TARGET_OBJECTS:softfloat-3>)
endif()
add_dependencies (tlib tcg)

if("${TARGET_ACTUAL_ARCH}" STREQUAL "i386")
    set (MATH_LIB_LINK_ARG "-lm" CACHE STRING
      "Argument pointing linker to a math functions library. It's required to translate i386 code.")
endif()

# On x86_64 Linux, the memcpy function was modified in GNU libc v2.14.
# It'd be impossible to run tlib without this wrapping with older libc.
if(${CMAKE_HOST_SYSTEM_NAME} STREQUAL Linux AND ${HOST_ARCH} STREQUAL i386 AND ${HOST_WORD_SIZE} EQUAL 64)
    set(WRAP_MEMCPY_OPT -Wl,--wrap=memcpy)
endif()

# -z flag does not work windows
if ((NOT (${CMAKE_HOST_SYSTEM_NAME} MATCHES "(Windows|CYGWIN)" )) AND ("${C_COMPILER_ID}" STREQUAL "GNU"))
    set (Z_OPTS -zdefs)
endif()

target_link_libraries (tlib
    ${WRAP_MEMCPY_OPT}
    -fPIC
    ${Z_OPTS}

    ${MATH_LIB_LINK_ARG}
    softfloat-3
    pthread
    tcg
    )
