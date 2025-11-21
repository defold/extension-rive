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
TARGET_SHARE_DIR="${SCRIPT_DIR}/../defold-rive/plugins/share"

if [ -z "${DYNAMO_HOME:-}" ]; then
    echo "DYNAMO_HOME must be set before running $0" >&2
    exit 1
fi

DYNAMO_HOME="$(realpath "${DYNAMO_HOME}")"
export DYNAMO_HOME

if [ -z "${BOB:-}" ]; then
    echo "BOB must be set before running $0" >&2
    exit 1
fi

BOB="$(realpath "${BOB}")"
if [ ! -f "${BOB}" ]; then
    echo "BOB jar not found at ${BOB}" >&2
    exit 1
fi
export BOB

case "$(uname -s)" in
    Darwin)
        if [ "$(uname -m)" = "arm64" ]; then
            HOST_PLATFORM="arm64-macos"
        else
            HOST_PLATFORM="x86_64-macos"
        fi
        ;;
    Linux)
        HOST_PLATFORM="x86_64-linux"
        ;;
    MINGW*|MSYS*|CYGWIN*|Windows_NT)
        HOST_PLATFORM="x86_64-win32"
        ;;
    *)
        HOST_PLATFORM="x86_64-linux"
        ;;
esac

PROTOBUF_BIN="${SCRIPT_DIR}/../build/protobuf-3.20.1-${HOST_PLATFORM}/bin/${HOST_PLATFORM}"
if [ -d "${PROTOBUF_BIN}" ]; then
    export PATH="${PROTOBUF_BIN}:${PATH}"
else
    echo "Warning: protobuf bin directory not found at ${PROTOBUF_BIN}" >&2
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

mkdir -p ${TARGET_SHARE_DIR}
cp -v ${BUILD_DIR}/pluginRiveExt.jar ${TARGET_SHARE_DIR}


echo "Done."
