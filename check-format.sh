#!/usr/bin/env bash

CLANG_FORMAT_COMMAND=${1:-clang-format-19}

TLIB_DIR="$(dirname $0)"

TEMP_FILE=$(mktemp)

NON_CONFORMING_FILES=0
for file in $(find $TLIB_DIR -name '*.c' -o -name '*.h'| grep -v softfloat-3); do
    echo "Checking $file"
    FORMATTING_OUTPUT=$(${CLANG_FORMAT_COMMAND} --style=file --dry-run --Werror --verbose $file 2>${TEMP_FILE})
    COMMAND_EXIT_CODE=$?
    OUTPUT_ERRORS=$(grep error ${TEMP_FILE} | wc -l)
    if [ ${OUTPUT_ERRORS} -ne 0 ] || [ ${COMMAND_EXIT_CODE} -ne 0 ] ; then
        if [ ${COMMAND_EXIT_CODE} -ne 0 ]; then
            echo "Clang-format exited with code ${COMMAND_EXIT_CODE}!!"
        fi
        cat ${TEMP_FILE}
        ((NON_CONFORMING_FILES++))
    fi
done

if [ ${NON_CONFORMING_FILES} -ne 0 ]; then
    echo "Clang-format found non-conforming files: ${NON_CONFORMING_FILES}" >&2
    exit 1
fi
