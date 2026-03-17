#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" >/dev/null && pwd)"

WITH_ASAN=OFF
WITH_VULKAN=""
WITH_OPENGL=""
BUILD_CONFIG=RelWithDebInfo
RIVE_LIB_DIR=""
ARGS=()

parse_on_off_flag() {
    local value="$1"
    case "${value}" in
        1|ON|on|TRUE|true|YES|yes)
            echo "ON"
            ;;
        0|OFF|off|FALSE|false|NO|no)
            echo "OFF"
            ;;
        *)
            echo "Invalid boolean value '${value}'. Expected one of: ON/OFF, true/false, yes/no, 1/0" >&2
            exit 1
            ;;
    esac
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --with-asan)
            WITH_ASAN=ON
            if [[ $# -ge 2 ]]; then
                case "$2" in
                    1|0|ON|on|OFF|off|TRUE|true|FALSE|false|YES|yes|NO|no)
                        WITH_ASAN="$(parse_on_off_flag "$2")"
                        shift 2
                        continue
                        ;;
                esac
            fi
            shift
            ;;
        --with-asan=*)
            WITH_ASAN="$(parse_on_off_flag "${1#*=}")"
            shift
            ;;
        --with_vulkan|--with-vulkan)
            WITH_VULKAN=ON
            shift
            ;;
        --with_opengl|--with-opengl)
            WITH_OPENGL=ON
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
            if [[ "$1" == --* ]]; then
                echo "Unknown option: $1"
                echo "Use --with-vulkan or --with-opengl"
                exit 1
            fi
            ARGS+=("$1")
            shift
            ;;
    esac
done

if [[ ${#ARGS[@]} -gt 1 ]]; then
    echo "Too many positional arguments: ${ARGS[*]}"
    echo "Usage: $0 [options] <target_platform>"
    exit 1
fi

TARGET_PLATFORM="${ARGS[0]:-${TARGET_PLATFORM:-}}"

if [ -z "${TARGET_PLATFORM}" ]; then
    echo "Missing required <target_platform> (or set TARGET_PLATFORM in env)."
    echo "Usage: $0 [--with-asan] [--with-vulkan|--with-opengl] [--config <Debug|Release|RelWithDebInfo|MinSizeRel>] [--rive-lib-dir <path>] [--use-utils-libs-win64] <target_platform>"
    echo "Example: $0 --with-vulkan x86_64-linux"
    exit 1
fi

ALLOWED_PLATFORMS=("arm64-macos" "x86_64-win32" "x86_64-linux" "arm64-linux")
if [[ ! " ${ALLOWED_PLATFORMS[*]} " =~ " ${TARGET_PLATFORM} " ]]; then
    echo "Unsupported target platform: ${TARGET_PLATFORM}"
    echo "Supported platforms: ${ALLOWED_PLATFORMS[*]}"
    exit 1
fi

# Both cannot be set
if [[ "${WITH_VULKAN}" == "ON" && "${WITH_OPENGL}" == "ON" ]]; then
    echo "Conflicting backend options: both Vulkan and OpenGL were requested."
    exit 1
fi

if [[ "${WITH_OPENGL}" == "ON" ]]; then
    WITH_VULKAN=OFF
elif [[ "${WITH_VULKAN}" != "ON" ]]; then
    WITH_VULKAN=ON
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
    -DWITH_VULKAN=${WITH_VULKAN}
)
if [[ -n "${RIVE_LIB_DIR}" ]]; then
    CM_ARGS+=("-DVIEWER_WIN32_RIVE_LIB_DIR=${RIVE_LIB_DIR}")
fi

cmake -S "${SCRIPT_DIR}" -B "${BUILD_DIR}" "${GENERATOR_ARGS[@]}" "${CM_ARGS[@]}"
cmake --build "${BUILD_DIR}" --target viewer --config "${BUILD_CONFIG}"
