#!/usr/bin/env bash

# build_headers.sh
#
# Install Rive public headers into a PREFIX directory.
# Mirrors the header layout used by build_android.sh / build_emscripten.sh.
#
# Usage:
#   ./build_headers.sh --prefix /abs/path/to/prefix [--root /path/to/repo]
#
# Notes:
# - If --root is not provided, the current working directory is used as repo root.
# - Only files with extensions .h, .hpp, .hxx, .inl are copied.

set -euo pipefail

ROOT_DIR="$(pwd -P)"
PREFIX=""

print_help() {
    cat <<EOF
Install Rive public headers into a PREFIX directory.

Options:
  -p, --prefix PATH   Install prefix directory (required)
  -r, --root PATH     Repository root (default: current working directory)
  -h, --help          Show this help
EOF
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        -p|--prefix)
            PREFIX="${2:-}"
            shift 2
            ;;
        -r|--root)
            ROOT_DIR="${2:-}"
            shift 2
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

mkdir -p "$PREFIX"
PREFIX="$(cd "$PREFIX" && pwd -P)"

INCLUDE_DST="$PREFIX/include"
mkdir -p "$INCLUDE_DST"

echo "Installing headers to $INCLUDE_DST"

# Header copy map
declare -a HEADER_MAP=(
    "include:include" \
    "renderer/include/rive:include/rive" \
    "renderer/src/webgpu:include/rive/renderer/webgpu" \
    "renderer/glad:include/rive/renderer/gl" \
    "tess/include/rive:include/rive" \
    "decoders/include/rive:include/rive" \
    "renderer/glad/include/glad:include/glad" \
    "renderer/glad/include/KHR:include/KHR" \
)

for mapping in "${HEADER_MAP[@]}"; do
    src_rel="${mapping%%:*}"
    dst_rel="${mapping##*:}"
    src_dir="$ROOT_DIR/$src_rel/"
    dst_dir="$PREFIX/$dst_rel/"
    if [[ -d "$src_dir" ]]; then
        echo "  - $src_rel -> $dst_rel (headers only)"
        mkdir -p "$dst_dir"
        rsync -a -m \
            --include '*/' \
            --include '*.h' \
            --include '*.hpp' \
            --include '*.hxx' \
            --include '*.inl' \
            --exclude '*' \
            "$src_dir" "$dst_dir"
    fi
done

echo "Headers installed to $INCLUDE_DST"
