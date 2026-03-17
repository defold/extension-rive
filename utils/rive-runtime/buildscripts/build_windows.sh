#!/usr/bin/env bash

# build_windows.sh
#
# Build Rive C++ static libraries for Windows and install into a PREFIX dir.
# Intended for CI use on a Windows runner (Git Bash), with MSVC toolchain
# available (e.g., Visual Studio 2022 on windows-latest).
#
# Usage:
#   ./build_windows.sh --prefix /abs/path/to/prefix \
#       [--archs x64,x86] [--config release|debug]
#
# Notes:
# - Uses the repo's build/build_rive.sh (premake+ninja) to produce libraries.
# - Installs headers to "$PREFIX/include" and libs to
#   "$PREFIX/lib/<arch>-win32" where <arch> is x86 or x86_64.

set -euo pipefail
shopt -s nullglob

# Treat the current working directory as the repo root (do not depend on script location).
ROOT_DIR="$(pwd -P)"
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"

PREFIX=""
ARCHS="x64,x86"
CONFIG="release"
WITH_VULKAN=false

print_help() {
    cat <<EOF
Build Windows static libraries and install to a PREFIX directory.

Options:
  -p, --prefix PATH    Install prefix directory (required)
  -a, --archs LIST     Comma/space-separated: x64, x86 (default: x64,x86)
  -c, --config NAME    Build config: release|debug (default: release)
      --with-vulkan    Forward --with_vulkan to build_rive.sh
  -h, --help           Show this help

Examples:
  ./build_windows.sh --prefix /tmp/rive-win --archs x64,x86 --config release
EOF
}

# Parse args
while [[ $# -gt 0 ]]; do
    case "$1" in
        -p|--prefix)
            PREFIX="${2:-}"
            shift 2
            ;;
        -a|--archs)
            ARCHS="${2:-}"
            shift 2
            ;;
        -c|--config)
            CONFIG="${2:-}"
            shift 2
            ;;
        --with-vulkan|--with_vulkan)
            WITH_VULKAN=true
            shift
            ;;
        -h|--help)
            print_help
            exit 0
            ;;
        *)
            echo "Unknown argument: $1" >&2
            echo >&2
            print_help >&2
            exit 2
            ;;
    esac
done

if [[ -z "$PREFIX" ]]; then
    echo "error: --prefix is required" >&2
    echo >&2
    print_help >&2
    exit 2
fi

# Ensure PREFIX is an absolute, existing path
mkdir -p "$PREFIX"
PREFIX="$(cd "$PREFIX" && pwd -P)"

if [[ "$CONFIG" != "release" && "$CONFIG" != "debug" ]]; then
    echo "error: --config must be 'release' or 'debug'" >&2
    exit 2
fi

if ! command -v msbuild.exe >/dev/null 2>&1; then
    MSBUILD_DIR=""
    VSWHERE="/c/Program Files (x86)/Microsoft Visual Studio/Installer/vswhere.exe"

    if [[ -x "$VSWHERE" ]]; then
        MSBUILD_PATH="$("$VSWHERE" -latest -products '*' -requires Microsoft.Component.MSBuild -find 'MSBuild\**\Bin\MSBuild.exe' 2>/dev/null | head -n 1 | tr -d '\r')"
        if [[ -n "$MSBUILD_PATH" && -f "$MSBUILD_PATH" ]]; then
            MSBUILD_DIR="$(cd "$(dirname "$MSBUILD_PATH")" && pwd -P)"
        fi
    fi

    if [[ -z "$MSBUILD_DIR" ]]; then
        for dir in \
            "/c/Program Files/Microsoft Visual Studio/2022/Community/MSBuild/Current/Bin" \
            "/c/Program Files/Microsoft Visual Studio/2022/Community/MSBuild/Current/Bin/amd64" \
            "/c/Program Files/Microsoft Visual Studio/2022/BuildTools/MSBuild/Current/Bin" \
            "/c/Program Files/Microsoft Visual Studio/2022/BuildTools/MSBuild/Current/Bin/amd64"
        do
            if [[ -f "$dir/MSBuild.exe" ]]; then
                MSBUILD_DIR="$dir"
                break
            fi
        done
    fi

    if [[ -n "$MSBUILD_DIR" ]]; then
        export PATH="$MSBUILD_DIR:$PATH"
        echo "Added MSBuild to PATH from: $MSBUILD_DIR"
    fi
fi

if ! command -v msbuild.exe >/dev/null 2>&1; then
    echo "error: msbuild.exe not found in PATH. Install Visual Studio Build Tools and ensure MSBuild.exe is available." >&2
    exit 2
fi

if ! command -v fxc >/dev/null 2>&1; then
    WIN_KITS_ROOT="/c/Program Files (x86)/Windows Kits/10/bin"
    if [[ -d "$WIN_KITS_ROOT" ]]; then
        # Pick the newest available SDK version with x64 fxc.exe.
        FXC_DIR="$(ls -d "$WIN_KITS_ROOT"/*/x64 2>/dev/null | sort -V | tail -n 1 || true)"
        if [[ -n "$FXC_DIR" && -f "$FXC_DIR/fxc.exe" ]]; then
            export PATH="$FXC_DIR:$PATH"
            echo "Added fxc to PATH from Windows Kits: $FXC_DIR"
        fi
    fi
fi

if ! command -v fxc >/dev/null 2>&1; then
    echo "error: fxc not found in PATH. Install Windows SDK and ensure fxc.exe is available." >&2
    exit 2
fi

if ! command -v glslangValidator >/dev/null 2>&1; then
    if [[ -n "${GLSLANG_VALIDATOR:-}" && -f "${GLSLANG_VALIDATOR}" ]]; then
        GLSLANG_DIR="$(cd "$(dirname "${GLSLANG_VALIDATOR}")" && pwd -P)"
        export PATH="$GLSLANG_DIR:$PATH"
        echo "Added glslangValidator to PATH from GLSLANG_VALIDATOR: $GLSLANG_DIR"
    fi
fi

if ! command -v glslangValidator >/dev/null 2>&1; then
    CANDIDATE_DIRS=()

    if [[ -n "${VULKAN_SDK:-}" ]]; then
        CANDIDATE_DIRS+=("$VULKAN_SDK/Bin" "$VULKAN_SDK/Bin32")
    fi

    if [[ -d "/c/VulkanSDK" ]]; then
        while IFS= read -r _dir; do CANDIDATE_DIRS+=("$_dir"); done < <(ls -d /c/VulkanSDK/*/Bin /c/VulkanSDK/*/Bin32 2>/dev/null | sort -V)
    fi
    if [[ -d "/c/Program Files/VulkanSDK" ]]; then
        while IFS= read -r _dir; do CANDIDATE_DIRS+=("$_dir"); done < <(ls -d "/c/Program Files/VulkanSDK"/*/Bin "/c/Program Files/VulkanSDK"/*/Bin32 2>/dev/null | sort -V)
    fi

    CANDIDATE_DIRS+=("/c/Program Files/RenderDoc/plugins/spirv")

    for dir in "${CANDIDATE_DIRS[@]}"; do
        if [[ -f "$dir/glslangValidator.exe" || -f "$dir/glslangValidator" ]]; then
            export PATH="$dir:$PATH"
            echo "Added glslangValidator to PATH from: $dir"
            break
        fi
    done
fi

if ! command -v glslangValidator >/dev/null 2>&1; then
    echo "error: glslangValidator not found in PATH." >&2
    echo "Set VULKAN_SDK or GLSLANG_VALIDATOR, or install Vulkan SDK / RenderDoc with glslangValidator." >&2
    exit 2
fi

if ! command -v spirv-opt >/dev/null 2>&1; then
    SPIRV_CANDIDATE_DIRS=()

    if [[ -n "${SPIRV_TOOLS_BIN:-}" ]]; then
        SPIRV_CANDIDATE_DIRS+=("${SPIRV_TOOLS_BIN}")
    fi

    if [[ -n "${DYNAMO_HOME:-}" ]]; then
        SPIRV_CANDIDATE_DIRS+=("${DYNAMO_HOME}/ext/bin/x86_64-win32")
    fi

    if [[ -d "/c/repos/defold/tmp/dynamo_home/ext/bin/x86_64-win32" ]]; then
        SPIRV_CANDIDATE_DIRS+=("/c/repos/defold/tmp/dynamo_home/ext/bin/x86_64-win32")
    fi

    while IFS= read -r _dir; do SPIRV_CANDIDATE_DIRS+=("$_dir"); done < <(ls -d /c/repos/*/tmp/dynamo_home/ext/bin/x86_64-win32 2>/dev/null | sort -V)
    while IFS= read -r _dir; do SPIRV_CANDIDATE_DIRS+=("$_dir"); done < <(ls -d /c/repos/*/com.dynamo.cr/com.dynamo.cr.bob/libexec/x86_64-win32 2>/dev/null | sort -V)

    for dir in "${SPIRV_CANDIDATE_DIRS[@]}"; do
        if [[ -f "$dir/spirv-opt.exe" || -f "$dir/spirv-opt" ]]; then
            export PATH="$dir:$PATH"
            echo "Added spirv-opt to PATH from: $dir"
            break
        fi
    done
fi

if ! command -v spirv-opt >/dev/null 2>&1; then
    echo "error: spirv-opt not found in PATH." >&2
    echo "Set SPIRV_TOOLS_BIN or DYNAMO_HOME, or provide spirv-tools binaries on PATH." >&2
    exit 2
fi

BUILD_SCRIPT="$ROOT_DIR/build/build_rive.sh"
if [[ ! -x "$BUILD_SCRIPT" ]]; then
    if [[ -f "$BUILD_SCRIPT" ]]; then
        chmod +x "$BUILD_SCRIPT"
    else
        echo "error: build script not found: $BUILD_SCRIPT" >&2
        exit 2
    fi
fi

# Normalize arch list
IFS="," read -r -a ARCH_LIST_RAW <<< "$ARCHS"
ARCH_LIST=()
for entry in "${ARCH_LIST_RAW[@]}"; do
    for a in $entry; do
        if [[ -n "$a" ]]; then
            case "$a" in
                x64|x86) ARCH_LIST+=("$a") ;;
                *) echo "error: unsupported arch '$a' (supported: x64, x86)" >&2; exit 2 ;;
            esac
        fi
    done
done

# Ensure prefix subdirs (headers handled by build_headers.sh)
INCLUDE_DST="$PREFIX/include"
LIB_ROOT="$PREFIX/lib"
mkdir -p "$INCLUDE_DST" "$LIB_ROOT"

# Install headers via shared helper
"${SCRIPT_DIR}/build_headers.sh" --prefix "$PREFIX" --root "$ROOT_DIR"

# Change into renderer directory before building so premake picks up renderer/premake5.lua
BUILD_DIR="$ROOT_DIR/renderer"
echo "Changing directory to: $BUILD_DIR"
cd "$BUILD_DIR"

for ARCH in "${ARCH_LIST[@]}"; do
    echo
    echo "==> Building: windows $ARCH ($CONFIG)"

    out_dir_rel="out/windows_${ARCH}_${CONFIG}"
    out_dir="$BUILD_DIR/$out_dir_rel"

    # Drive build (use windows token and arch)
    BUILD_ARGS=(--toolset=msc "$ARCH" "$CONFIG")
    if [[ "$WITH_VULKAN" == "true" ]]; then
        BUILD_ARGS+=(--with_vulkan)
    fi
    RIVE_OUT="$out_dir_rel" "$BUILD_SCRIPT" "${BUILD_ARGS[@]}"

    # Collect and install libraries
    install_arch="$ARCH"
    if [[ "$install_arch" == "x64" ]]; then install_arch="x86_64"; fi
    if [[ "$install_arch" == "x86" ]]; then install_arch="x86"; fi
    install_dir="$LIB_ROOT/${install_arch}-win32"
    mkdir -p "$install_dir"

    libs=( "$out_dir"/lib*.a "$out_dir"/*.lib "$out_dir"/*.dll )
    if (( ${#libs[@]} == 0 )); then
        echo "error: no libraries found in $out_dir (expected *.lib or lib*.a)" >&2
        exit 2
    fi

    echo "Installing libraries to $install_dir/:"
    for lib in "${libs[@]}"; do
        echo "  - $(basename "$lib")"
        cp -f "$lib" "$install_dir/"
    done
done

echo
echo "Done. Installed to: $PREFIX"
