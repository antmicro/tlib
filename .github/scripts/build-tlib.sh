#!/usr/bin/env bash

set -e

ROOT_PATH="$(cd "$(dirname $0)"; echo $PWD)/../.."

CORES_BUILD_PATH=$ROOT_PATH/obj

function build_core () {
    core_config=$1

    CORE="$(echo $core_config | cut -d '.' -f 1)"
    ENDIAN="$(echo $core_config | cut -d '.' -f 2)"
    BITS=32

    if [[ $CORE =~ "64" ]]; then
      BITS=64
    fi

    # Core specific flags to cmake
    CMAKE_CONF_FLAGS="-DTARGET_ARCH=$CORE -DTARGET_WORD_SIZE=$BITS"
    CORE_DIR=$CORES_BUILD_PATH/$CORE/$ENDIAN
    mkdir -p $CORE_DIR
    pushd "$CORE_DIR" > /dev/null

    if [[ $ENDIAN == "be" ]]; then
        CMAKE_CONF_FLAGS+=" -DTARGET_WORDS_BIGENDIAN=1"
    fi

    cmake $CMAKE_CONF_FLAGS $ROOT_PATH
    cmake --build . -j"$(nproc)"

    popd > /dev/null
}

. $ROOT_PATH/cores.sh

for core_config in "${CORES[@]}"
do
    build_core $core_config
done
