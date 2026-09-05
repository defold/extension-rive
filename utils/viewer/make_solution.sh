#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" >/dev/null && pwd)"

BUILD_CONFIG="RelWithDebInfo"
DO_BUILD=0
BUILD_DIR="${SCRIPT_DIR}/build/x86_64-win32"
TARGET_PLATFORM="x86_64-win32"
RIVE_LIB_DIR=""

select_visual_studio_generator() {
    local cmake_help
    cmake_help="$(cmake --help 2>/dev/null || true)"
    local preferred_generators=()
    local visual_studio_version="${VisualStudioVersion:-}"
    local detected_vs_major="${visual_studio_version%%.*}"

    if [[ -z "${detected_vs_major}" || "${detected_vs_major}" == "${visual_studio_version}" ]]; then
        local vswhere_cmd=""
        if command -v vswhere.exe >/dev/null 2>&1; then
            vswhere_cmd="$(command -v vswhere.exe)"
        elif command -v vswhere >/dev/null 2>&1; then
            vswhere_cmd="$(command -v vswhere)"
        fi
        if [[ -n "${vswhere_cmd}" ]]; then
            local installed_vs_version
            installed_vs_version="$("${vswhere_cmd}" -latest -property installationVersion 2>/dev/null | tr -d '\r' || true)"
            detected_vs_major="${installed_vs_version%%.*}"
        fi
    fi

    case "${detected_vs_major}" in
        18)
            preferred_generators+=("Visual Studio 18 2026" "Visual Studio 17 2022")
            ;;
        17)
            preferred_generators+=("Visual Studio 17 2022" "Visual Studio 18 2026")
            ;;
        *)
            preferred_generators+=("Visual Studio 18 2026" "Visual Studio 17 2022")
            ;;
    esac

    local generator
    for generator in "${preferred_generators[@]}"; do
        if grep -Fq "${generator}" <<< "${cmake_help}"; then
            echo "${generator}"
            return 0
        fi
    done

    echo "Unable to find a supported Visual Studio CMake generator." >&2
    echo "Install Visual Studio 2022/2026 build tools or set CMAKE_GENERATOR explicitly." >&2
    exit 1
}

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

if [[ -n "${CMAKE_GENERATOR:-}" ]]; then
    VS_GENERATOR="${CMAKE_GENERATOR}"
    echo "Using user-specified CMAKE_GENERATOR='${VS_GENERATOR}'"
else
    VS_GENERATOR="$(select_visual_studio_generator)"
    echo "Using CMake generator '${VS_GENERATOR}' for ${TARGET_PLATFORM}"
fi

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
