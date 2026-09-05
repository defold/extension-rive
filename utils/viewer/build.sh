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
COMPILER_ARGS=()
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

    if [[ -z "${CMAKE_GENERATOR:-}" ]]; then
        if { command -v ninja >/dev/null 2>&1 || command -v ninja.exe >/dev/null 2>&1; } &&
            { command -v cl >/dev/null 2>&1 || command -v cl.exe >/dev/null 2>&1; }; then
            echo "Using CMake generator 'Ninja' with MSVC for ${TARGET_PLATFORM}"
            GENERATOR_ARGS=(-G Ninja)
            COMPILER_ARGS+=("-DCMAKE_C_COMPILER=cl")
            COMPILER_ARGS+=("-DCMAKE_CXX_COMPILER=cl")
        else
            VS_GENERATOR="$(select_visual_studio_generator)"
            echo "Using CMake generator '${VS_GENERATOR}' for ${TARGET_PLATFORM}"
            GENERATOR_ARGS=(-G "${VS_GENERATOR}" -A x64)
        fi
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
    -DWITH_OPENGL=${WITH_OPENGL:-OFF}
)
if [[ -n "${RIVE_LIB_DIR}" ]]; then
    CM_ARGS+=("-DVIEWER_WIN32_RIVE_LIB_DIR=${RIVE_LIB_DIR}")
fi

CMAKE_CONFIGURE_ARGS=(-S "${SCRIPT_DIR}" -B "${BUILD_DIR}")
if [[ ${#GENERATOR_ARGS[@]} -gt 0 ]]; then
    CMAKE_CONFIGURE_ARGS+=("${GENERATOR_ARGS[@]}")
fi
if [[ ${#COMPILER_ARGS[@]} -gt 0 ]]; then
    CMAKE_CONFIGURE_ARGS+=("${COMPILER_ARGS[@]}")
fi
CMAKE_CONFIGURE_ARGS+=("${CM_ARGS[@]}")

cmake "${CMAKE_CONFIGURE_ARGS[@]}"
cmake --build "${BUILD_DIR}" --target viewer --config "${BUILD_CONFIG}"
