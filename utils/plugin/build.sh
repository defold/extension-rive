#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"
REPO_ROOT=$(realpath ${SCRIPT_DIR}/../..)

if [ $# -lt 1 ]; then
    echo "Usage: $0 <platform>"
    exit 1
fi

PLATFORM="$1"
CONFIG="${CONFIG:-RelWithDebInfo}"
BUILD_DIR="${SCRIPT_DIR}/build/${PLATFORM}"

EXTENDER_PLATFORM="${PLATFORM}"
case $PLATFORM in
    "arm64-macos")
        EXTENDER_PLATFORM="arm64-osx"
        ;;
   "x86_64-macos")
        EXTENDER_PLATFORM="x86_64-osx"
        ;;
esac
TARGET_LIB_DIR="${REPO_ROOT}/defold-rive/plugins/lib/${EXTENDER_PLATFORM}"
TARGET_SHARE_DIR="${REPO_ROOT}/defold-rive/plugins/share"
EDITOR_PLUGIN_ROOT="${REPO_ROOT}/build/plugins/defold-rive/plugins"
EDITOR_TARGET_LIB_DIR="${EDITOR_PLUGIN_ROOT}/lib/${EXTENDER_PLATFORM}"
EDITOR_TARGET_SHARE_DIR="${EDITOR_PLUGIN_ROOT}/share"

if [ -z "${DYNAMO_HOME:-}" ]; then
    echo "DYNAMO_HOME must be set before running $0" >&2
    exit 1
fi

export CMAKE_C_COMPILER="$(which clang)"
export CMAKE_CXX_COMPILER="$(which clang++)"

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

CMAKE_GENERATOR_FLAGS=()
if [ "$HOST_PLATFORM" = "x86_64-win32" ]; then
    CMAKE_GENERATOR_FLAGS+=("-A" "x64")
fi

CMAKE_PROTOC_ARGS=()

if [ -n "${PROTOC:-}" ] && [ -x "${PROTOC}" ]; then
    PROTOC_DIR="$(dirname "${PROTOC}")"
    export PATH="${PROTOC_DIR}:${PATH}"
    CMAKE_PROTOC_ARGS+=("-DPROTOC_EXECUTABLE=${PROTOC}")
elif command -v protoc >/dev/null 2>&1; then
    PROTOC_PATH="$(command -v protoc)"
    PROTOC_DIR="$(dirname "${PROTOC_PATH}")"
    export PATH="${PROTOC_DIR}:${PATH}"
    CMAKE_PROTOC_ARGS+=("-DPROTOC_EXECUTABLE=${PROTOC_PATH}")
else
    #PROTOBUF_BIN="${REPO_ROOT}/build/bin/${HOST_PLATFORM}"
    PROTOBUF_BIN="${DYNAMO_HOME}/ext/bin/${HOST_PLATFORM}"
    if [ -d "${PROTOBUF_BIN}" ]; then
        export PATH="${PROTOBUF_BIN}:${PATH}"
        if [ -x "${PROTOBUF_BIN}/protoc" ]; then
            CMAKE_PROTOC_ARGS+=("-DPROTOC_EXECUTABLE=${PROTOBUF_BIN}/protoc")
        elif [ -x "${PROTOBUF_BIN}/protoc.exe" ]; then
            CMAKE_PROTOC_ARGS+=("-DPROTOC_EXECUTABLE=${PROTOBUF_BIN}/protoc.exe")
        fi
    else
        echo "Warning: protobuf bin directory not found at ${PROTOBUF_BIN}" >&2
        echo "build folder: ${REPO_ROOT}/build"
        ls -la "${REPO_ROOT}/build"
    fi
fi

mkdir -p "${BUILD_DIR}"

cmake -S "${SCRIPT_DIR}" -B "${BUILD_DIR}" \
    -DTARGET_PLATFORM="${PLATFORM}" \
    -DCMAKE_BUILD_TYPE="${CONFIG}" \
    -DCMAKE_VERBOSE_MAKEFILE=ON \
    -DCMAKE_C_COMPILER="${CMAKE_C_COMPILER}" \
    -DCMAKE_CXX_COMPILER="${CMAKE_CXX_COMPILER}" \
    "${CMAKE_PROTOC_ARGS[@]:-}" \
    "${CMAKE_GENERATOR_FLAGS[@]:-}"

cmake --build "${BUILD_DIR}" --config "${CONFIG}"

copy_plugin_libs() {
    local dst_dir="$1"
    mkdir -p "${dst_dir}"
    case $PLATFORM in
        "arm64-macos"|"x86_64-macos")
            cp -v "${BUILD_DIR}"/*.dylib "${dst_dir}"
            ;;
        "arm64-linux"|"x86_64-linux")
            cp -v "${BUILD_DIR}"/*.so "${dst_dir}"
            ;;
        "x86_64-win32")
            if compgen -G "${BUILD_DIR}/${CONFIG}/*.dll" > /dev/null; then
                cp -v "${BUILD_DIR}/${CONFIG}"/*.dll "${dst_dir}"
            else
                cp -v "${BUILD_DIR}"/*.dll "${dst_dir}"
            fi
            ;;
    esac
}

copy_plugin_libs "${TARGET_LIB_DIR}"

mkdir -p ${TARGET_SHARE_DIR}
cp -v ${BUILD_DIR}/pluginRiveExt.jar ${TARGET_SHARE_DIR}

if [ -d "${EDITOR_PLUGIN_ROOT}" ]; then
    mkdir -p "${EDITOR_TARGET_SHARE_DIR}"
    cp -v "${BUILD_DIR}/pluginRiveExt.jar" "${EDITOR_TARGET_SHARE_DIR}"

    if ! copy_plugin_libs "${EDITOR_TARGET_LIB_DIR}"; then
        echo "Warning: failed to copy editor plugin libraries to ${EDITOR_TARGET_LIB_DIR}" >&2
        echo "Warning: the editor may still be running and locking libRiveExt.dll. Close editor and rebuild." >&2
    fi
fi

echo "Done."
