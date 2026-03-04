#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" >/dev/null && pwd)"

BUILD_CONFIG="RelWithDebInfo"
DO_BUILD=0
BUILD_DIR="${SCRIPT_DIR}/build/x86_64-win32"
TARGET_PLATFORM="x86_64-win32"
VS_GENERATOR="Visual Studio 17 2022"
RIVE_LIB_DIR=""

usage() {
    echo "Usage: $0 [--config <Debug|Release|RelWithDebInfo|MinSizeRel>] [--build|--no-build] [--build-dir <dir>] [--rive-lib-dir <path>] [--use-utils-libs-win64]"
    echo
    echo "Creates a Visual Studio solution for the viewer (x86_64-win32)."
    echo "Default build config: RelWithDebInfo"
    echo "Default behavior: generate solution only (no build)."
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --config)
            if [[ $# -lt 2 ]]; then
                echo "Missing value for --config"
                exit 1
            fi
            BUILD_CONFIG="$2"
            shift 2
            ;;
        --build)
            DO_BUILD=1
            shift
            ;;
        --no-build)
            DO_BUILD=0
            shift
            ;;
        --build-dir)
            if [[ $# -lt 2 ]]; then
                echo "Missing value for --build-dir"
                exit 1
            fi
            BUILD_DIR="$2"
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
        -h|--help)
            usage
            exit 0
            ;;
        *)
            echo "Unknown argument: $1"
            usage
            exit 1
            ;;
    esac
done

UNAME_S="$(uname -s)"
case "${UNAME_S}" in
    MINGW*|MSYS*|CYGWIN*|Windows_NT)
        ;;
    *)
        echo "This script is for Windows hosts only."
        exit 1
        ;;
esac

if [[ -z "${DYNAMO_HOME:-}" ]]; then
    echo "DYNAMO_HOME is required."
    exit 1
fi

mkdir -p "${BUILD_DIR}"

CM_ARGS=(
    -DTARGET_PLATFORM="${TARGET_PLATFORM}"
)
if [[ -n "${RIVE_LIB_DIR}" ]]; then
    CM_ARGS+=("-DVIEWER_WIN32_RIVE_LIB_DIR=${RIVE_LIB_DIR}")
fi

cmake -S "${SCRIPT_DIR}" -B "${BUILD_DIR}" \
    -G "${VS_GENERATOR}" \
    -A x64 \
    "${CM_ARGS[@]}"

if [[ "${DO_BUILD}" -eq 1 ]]; then
    cmake --build "${BUILD_DIR}" --target viewer --config "${BUILD_CONFIG}"
fi

echo "Solution generated: ${BUILD_DIR}/viewer.sln"
