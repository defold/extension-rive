#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" >/dev/null && pwd)"

WITH_ASAN=OFF
BUILD_CONFIG=RelWithDebInfo
RIVE_LIB_DIR=""
ARGS=()

while [[ $# -gt 0 ]]; do
    case "$1" in
        --with-asan)
            WITH_ASAN=ON
            shift
            ;;
        --config)
            if [[ $# -lt 2 ]]; then
                echo "Missing value for --config"
                exit 1
            fi
            BUILD_CONFIG="$2"
            shift 2
            ;;
        --rive-lib-dir)
            if [[ $# -lt 2 ]]; then
                echo "Missing value for --rive-lib-dir"
                exit 1
            fi
            RIVE_LIB_DIR="$2"
            shift 2
            ;;
        --use-utils-libs-win64)
            RIVE_LIB_DIR="${SCRIPT_DIR}/../libs_win64"
            shift
            ;;
        *)
            ARGS+=("$1")
            shift
            ;;
    esac
done

TARGET_PLATFORM="${ARGS[0]:-${TARGET_PLATFORM:-}}"

if [ -z "${TARGET_PLATFORM}" ]; then
    echo "Usage: $0 [--with-asan] [--config <Debug|Release|RelWithDebInfo|MinSizeRel>] [--rive-lib-dir <path>] [--use-utils-libs-win64] <target_platform>"
    exit 1
fi

ALLOWED_PLATFORMS=("arm64-macos" "x86_64-win32" "x86_64-linux" "arm64-linux")
if [[ ! " ${ALLOWED_PLATFORMS[*]} " =~ " ${TARGET_PLATFORM} " ]]; then
    echo "Unsupported target platform: ${TARGET_PLATFORM}"
    echo "Supported platforms: ${ALLOWED_PLATFORMS[*]}"
    exit 1
fi

BUILD_DIR="${SCRIPT_DIR}/build/${TARGET_PLATFORM}"
mkdir -p "${BUILD_DIR}"

GENERATOR_ARGS=(-G Ninja)
UNAME_S="$(uname -s)"
IS_WINDOWS_HOST=0
case "${UNAME_S}" in
    MINGW*|MSYS*|CYGWIN*|Windows_NT)
        IS_WINDOWS_HOST=1
        ;;
esac

if [[ "${TARGET_PLATFORM}" == "x86_64-win32" ]]; then
    if [[ "${IS_WINDOWS_HOST}" -ne 1 ]]; then
        echo "x86_64-win32 viewer build requires a Windows host with Visual Studio."
        exit 1
    fi

    # Force Visual Studio toolchain for Windows target to avoid picking MinGW/MSYS gcc.
    if [[ -z "${CMAKE_GENERATOR:-}" ]]; then
        GENERATOR_ARGS=(-G "Visual Studio 17 2022" -A x64)
    else
        echo "Using user-specified CMAKE_GENERATOR='${CMAKE_GENERATOR}'"
        GENERATOR_ARGS=()
    fi

    if [[ "${BUILD_CONFIG}" == "Debug" ]]; then
        echo "x86_64-win32 debug config is incompatible with prebuilt defold-rive libs."
        echo "Use --config RelWithDebInfo or --config Release."
        exit 1
    fi
fi

CM_ARGS=(
    -DCMAKE_BUILD_TYPE="${BUILD_CONFIG}"
    -DTARGET_PLATFORM="${TARGET_PLATFORM}"
    -DWITH_ASAN=${WITH_ASAN}
)
if [[ -n "${RIVE_LIB_DIR}" ]]; then
    CM_ARGS+=("-DVIEWER_WIN32_RIVE_LIB_DIR=${RIVE_LIB_DIR}")
fi

cmake -S "${SCRIPT_DIR}" -B "${BUILD_DIR}" "${GENERATOR_ARGS[@]}" "${CM_ARGS[@]}"
cmake --build "${BUILD_DIR}" --target viewer --config "${BUILD_CONFIG}"
