#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" >/dev/null && pwd)"

WITH_ASAN=OFF
BUILD_CONFIG=Debug
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
        *)
            ARGS+=("$1")
            shift
            ;;
    esac
done

TARGET_PLATFORM="${ARGS[0]:-${TARGET_PLATFORM:-}}"

if [ -z "${TARGET_PLATFORM}" ]; then
    echo "Usage: $0 [--with-asan] [--config <Debug|Release|RelWithDebInfo|MinSizeRel>] <target_platform>"
    exit 1
fi

ALLOWED_PLATFORMS=("arm64-macos" "x86_64-win32" "x86_64-linux")
if [[ ! " ${ALLOWED_PLATFORMS[*]} " =~ " ${TARGET_PLATFORM} " ]]; then
    echo "Unsupported target platform: ${TARGET_PLATFORM}"
    echo "Supported platforms: ${ALLOWED_PLATFORMS[*]}"
    exit 1
fi

BUILD_DIR="${SCRIPT_DIR}/build/${TARGET_PLATFORM}"
mkdir -p "${BUILD_DIR}"

cmake -S "${SCRIPT_DIR}" -B "${BUILD_DIR}" -G Ninja \
    -DCMAKE_BUILD_TYPE="${BUILD_CONFIG}" \
    -DTARGET_PLATFORM="${TARGET_PLATFORM}" \
    -DWITH_ASAN=${WITH_ASAN}
cmake --build "${BUILD_DIR}" --target viewer --config "${BUILD_CONFIG}"
