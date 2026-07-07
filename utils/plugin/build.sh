#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"
REPO_ROOT=$(realpath ${SCRIPT_DIR}/../..)

WITH_VULKAN=""
ARGS=()

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
        --with-vulkan|--with_vulkan)
            if [[ "${WITH_VULKAN}" == "OFF" ]]; then
                echo "Conflicting backend options: both Vulkan and OpenGL were requested." >&2
                exit 1
            fi
            WITH_VULKAN=ON
            shift
            ;;
        --with-opengl|--with_opengl)
            if [[ "${WITH_VULKAN}" == "ON" ]]; then
                echo "Conflicting backend options: both Vulkan and OpenGL were requested." >&2
                exit 1
            fi
            WITH_VULKAN=OFF
            shift
            ;;
        *)
            ARGS+=("$1")
            shift
            ;;
    esac
done

if [[ ${#ARGS[@]} -ne 1 ]]; then
    echo "Usage: $0 [--with-vulkan|--with-opengl] <platform>"
    exit 1
fi

if [[ -z "${WITH_VULKAN}" ]]; then
    WITH_VULKAN=ON
fi

PLATFORM="${ARGS[0]}"
CONFIG="${CONFIG:-RelWithDebInfo}"
BUILD_DIR="${SCRIPT_DIR}/build/${PLATFORM}"
MACOS_DEPLOYMENT_TARGET="10.15"
MACOS_DYLIB_MINOS="${MACOS_DEPLOYMENT_TARGET}"

EXTENDER_PLATFORM="${PLATFORM}"
case $PLATFORM in
    "arm64-macos")
        EXTENDER_PLATFORM="arm64-osx"
        # Apple Silicon macOS starts at 11.0.
        MACOS_DEPLOYMENT_TARGET="11.0"
        MACOS_DYLIB_MINOS="${MACOS_DEPLOYMENT_TARGET}"
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
CMAKE_COMPILER_ARGS=()
if [ "$HOST_PLATFORM" = "x86_64-win32" ]; then
    if [[ -z "${CMAKE_GENERATOR:-}" ]]; then
        if { command -v ninja >/dev/null 2>&1 || command -v ninja.exe >/dev/null 2>&1; } &&
            { command -v cl >/dev/null 2>&1 || command -v cl.exe >/dev/null 2>&1; }; then
            echo "Using CMake generator 'Ninja' with MSVC for ${PLATFORM}"
            CMAKE_GENERATOR_FLAGS+=("-G" "Ninja")
            CMAKE_COMPILER_ARGS+=("-DCMAKE_C_COMPILER=cl")
            CMAKE_COMPILER_ARGS+=("-DCMAKE_CXX_COMPILER=cl")
        else
            VS_GENERATOR="$(select_visual_studio_generator)"
            echo "Using CMake generator '${VS_GENERATOR}' for ${PLATFORM}"
            CMAKE_GENERATOR_FLAGS+=("-G" "${VS_GENERATOR}" "-A" "x64")
        fi
    else
        echo "Using user-specified CMAKE_GENERATOR='${CMAKE_GENERATOR}'"
        if [[ "${CMAKE_GENERATOR}" == "Visual Studio"* && -z "${CMAKE_GENERATOR_PLATFORM:-}" ]]; then
            CMAKE_GENERATOR_FLAGS+=("-A" "x64")
        fi
    fi
else
    export CMAKE_C_COMPILER="${CMAKE_C_COMPILER:-$(which clang)}"
    export CMAKE_CXX_COMPILER="${CMAKE_CXX_COMPILER:-$(which clang++)}"
    CMAKE_COMPILER_ARGS+=("-DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}")
    CMAKE_COMPILER_ARGS+=("-DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}")
fi

CMAKE_PROTOC_ARGS=()

if [ -n "${PROTOC:-}" ] && [ -x "${PROTOC}" ]; then
    PROTOC_DIR="$(dirname "${PROTOC}")"
    export PATH="${PROTOC_DIR}:${PATH}"
    CMAKE_PROTOC_ARGS+=("-DPROTOC_EXECUTABLE=${PROTOC}")
else
    PROTOBUF_BIN="${DYNAMO_HOME}/ext/bin/${HOST_PLATFORM}"
    if [ -d "${PROTOBUF_BIN}" ]; then
        export PATH="${PROTOBUF_BIN}:${PATH}"
        if [ -x "${PROTOBUF_BIN}/protoc" ]; then
            CMAKE_PROTOC_ARGS+=("-DPROTOC_EXECUTABLE=${PROTOBUF_BIN}/protoc")
        elif [ -x "${PROTOBUF_BIN}/protoc.exe" ]; then
            CMAKE_PROTOC_ARGS+=("-DPROTOC_EXECUTABLE=${PROTOBUF_BIN}/protoc.exe")
        fi
    elif command -v protoc >/dev/null 2>&1; then
        PROTOC_PATH="$(command -v protoc)"
        PROTOC_DIR="$(dirname "${PROTOC_PATH}")"
        export PATH="${PROTOC_DIR}:${PATH}"
        CMAKE_PROTOC_ARGS+=("-DPROTOC_EXECUTABLE=${PROTOC_PATH}")
    else
        echo "Warning: protobuf bin directory not found at ${PROTOBUF_BIN}" >&2
        echo "build folder: ${REPO_ROOT}/build"
        ls -la "${REPO_ROOT}/build"
    fi
fi

mkdir -p "${BUILD_DIR}"

CM_ARGS=(
    -S "${SCRIPT_DIR}"
    -B "${BUILD_DIR}"
    -DTARGET_PLATFORM="${PLATFORM}"
    -DCMAKE_BUILD_TYPE="${CONFIG}"
    -DCMAKE_VERBOSE_MAKEFILE=ON
    -DWITH_VULKAN="${WITH_VULKAN}"
)

if [ ${#CMAKE_COMPILER_ARGS[@]} -gt 0 ]; then
    CM_ARGS+=("${CMAKE_COMPILER_ARGS[@]}")
fi

if [[ "$PLATFORM" == *"macos"* ]]; then
    export MACOSX_DEPLOYMENT_TARGET="${MACOS_DEPLOYMENT_TARGET}"
    CM_ARGS+=("-DCMAKE_OSX_DEPLOYMENT_TARGET=${MACOS_DEPLOYMENT_TARGET}")
fi

if [ ${#CMAKE_PROTOC_ARGS[@]} -gt 0 ]; then
    CM_ARGS+=("${CMAKE_PROTOC_ARGS[@]}")
fi
if [ ${#CMAKE_GENERATOR_FLAGS[@]} -gt 0 ]; then
    CM_ARGS+=("${CMAKE_GENERATOR_FLAGS[@]}")
fi

cmake "${CM_ARGS[@]}"

cmake --build "${BUILD_DIR}" --config "${CONFIG}"

verify_macos_dylib_deployment_target() {
    local dylib="$1"
    local minos

    if ! command -v otool >/dev/null 2>&1; then
        echo "error: otool is required to verify ${dylib}" >&2
        exit 1
    fi

    minos="$(otool -l "${dylib}" | awk '
        $1 == "cmd" && $2 == "LC_BUILD_VERSION" { in_build = 1; in_legacy = 0; next }
        in_build && $1 == "minos" { print $2; exit }
        $1 == "cmd" && $2 == "LC_VERSION_MIN_MACOSX" { in_build = 0; in_legacy = 1; next }
        in_legacy && $1 == "version" { print $2; exit }
        $1 == "cmd" { in_build = 0; in_legacy = 0 }
    ')"

    if [[ "${minos}" != "${MACOS_DYLIB_MINOS}" && "${minos}" != "${MACOS_DYLIB_MINOS}.0" ]]; then
        echo "error: ${dylib} has macOS minimum '${minos:-unknown}', expected ${MACOS_DYLIB_MINOS}" >&2
        exit 1
    fi
}

copy_plugin_libs() {
    local dst_dir="$1"
    mkdir -p "${dst_dir}"
    case $PLATFORM in
        "arm64-macos"|"x86_64-macos")
            for dylib in "${BUILD_DIR}"/*.dylib; do
                [ -e "${dylib}" ] || break
                verify_macos_dylib_deployment_target "${dylib}"
            done
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
