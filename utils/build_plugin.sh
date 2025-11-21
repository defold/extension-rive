#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"

if [ $# -lt 1 ]; then
    echo "Usage: $0 <platform>"
    exit 1
fi

PLATFORM="$1"
CONFIG="RelWithDebugInfo"
BUILD_DIR="${SCRIPT_DIR}/cmake/build/out_${PLATFORM}"

EXTENDER_PLATFORM="${PLATFORM}"
case $PLATFORM in
    "arm64-macos")
        EXTENDER_PLATFORM="arm64-osx"
        ;;
   "x86_64-macos")
        EXTENDER_PLATFORM="x86_64-osx"
        ;;
esac
TARGET_LIB_DIR="${SCRIPT_DIR}/../defold-rive/plugins/lib/${EXTENDER_PLATFORM}"
TARGET_SHARE_DIR="${SCRIPT_DIR}/../defold-rive/plugins/share/${EXTENDER_PLATFORM}"

if [ -z "${DYNAMO_HOME:-}" ]; then
    echo "DYNAMO_HOME must be set before running $0" >&2
    exit 1
fi

mkdir -p "${BUILD_DIR}"

cmake -S "${SCRIPT_DIR}/plugin" -B "${BUILD_DIR}" \
    -DTARGET_PLATFORM="${PLATFORM}" \
    -DCMAKE_BUILD_TYPE="${CONFIG}"

cmake --build "${BUILD_DIR}" --config "${CONFIG}"

case $PLATFORM in
    "arm64-macos"|"x86_64-macos")
        cp -v ${BUILD_DIR}/*.dylib ${TARGET_LIB_DIR}
        ;;
esac

#cp -v ${BUILD_DIR}/*.dylib ${TARGET_SHARE_DIR}


echo "Done."
