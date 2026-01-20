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

# Detect rsync availability and prepare a fallback copier.
if command -v rsync >/dev/null 2>&1; then
    RSYNC_AVAILABLE=true
else
    RSYNC_AVAILABLE=false
    echo "rsync not found; using fallback copy method" >&2
fi

copy_headers() {
    # copy_headers <src_dir/> <dst_dir/>
    local src="$1"
    local dst="$2"
    mkdir -p "$dst"
    if [[ "$RSYNC_AVAILABLE" == true ]]; then
        rsync -a -m \
            --include '*/' \
            --include '*.h' \
            --include '*.hpp' \
            --include '*.hxx' \
            --include '*.inl' \
            --exclude '*' \
            "$src" "$dst"
    else
        # Fallback: walk and copy only header-like files, recreating directories.
        # Ensure src ends with a slash for accurate prefix stripping.
        local src_trimmed="$src"
        [[ "${src_trimmed: -1}" != "/" ]] && src_trimmed+="/"
        # Use find -print0 to handle spaces.
        find "$src_trimmed" -type f \( \
            -name '*.h' -o -name '*.hpp' -o -name '*.hxx' -o -name '*.inl' \
        \) -print0 | while IFS= read -r -d '' f; do
            local rel="${f#${src_trimmed}}"
            local target_dir="$dst$(dirname "/$rel")"
            mkdir -p "$target_dir"
            cp -f "$f" "$dst$rel"
            echo "  + $rel"
        done
    fi
}

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
        copy_headers "$src_dir" "$dst_dir"
    fi
done

echo "Headers installed to $INCLUDE_DST"
