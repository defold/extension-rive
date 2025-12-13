#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" >/dev/null && pwd)"

WITH_ASAN=OFF
ARGS=()

while [[ $# -gt 0 ]]; do
    case "$1" in
        --with-asan)
            WITH_ASAN=ON
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
    echo "Usage: $0 [--with-asan] <target_platform>"
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
    -DCMAKE_BUILD_TYPE=Release \
    -DTARGET_PLATFORM="${TARGET_PLATFORM}" \
    -DWITH_ASAN=${WITH_ASAN}
cmake --build "${BUILD_DIR}" --target viewer
