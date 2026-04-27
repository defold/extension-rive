#!/usr/bin/env bash

# build_version_header.sh
#
# Write a small C header with Rive runtime version metadata based on the
# local Git repository HEAD, similar to write_rive_version_header.py but
# without using the GitHub API.
#
# Usage:
#   ./build_version_header.sh <output_header_path> [repo_dir]
#
# Example:
#   ./build_version_header.sh renderer/include/rive_runtime_version.h .

set -euo pipefail

if [[ $# -lt 1 ]]; then
    echo "Usage: $0 <output_header_path> [repo_dir]" >&2
    exit 2
fi

OUT_PATH="$1"
REPO_DIR="${2:-$(pwd -P)}"

# Ensure output directory exists
OUT_DIR="$(dirname "$OUT_PATH")"
mkdir -p "$OUT_DIR"

# Resolve Git information from local repo; fall back to .rive_head if needed.
sha=""
tag="${RIVE_RUNTIME_TAG:-}"
if git -C "$REPO_DIR" rev-parse --git-dir >/dev/null 2>&1; then
    sha=$(git -C "$REPO_DIR" rev-parse HEAD)
    if [[ -z "$tag" ]]; then
        tag=$(git -C "$REPO_DIR" describe --tags --exact-match HEAD 2>/dev/null || true)
    fi
    author=$(git -C "$REPO_DIR" show -s --format=%cn HEAD)
    date=$(git -C "$REPO_DIR" show -s --format=%cI HEAD)
    message=$(git -C "$REPO_DIR" log -1 --pretty=%B HEAD)
else
    if [[ -f "$REPO_DIR/.rive_head" ]]; then
        sha="$(cat "$REPO_DIR/.rive_head" | tr -d '\n\r')"
    else
        echo "error: not a git repo and .rive_head not found: $REPO_DIR" >&2
        exit 1
    fi
    if [[ -z "$tag" && -f "$REPO_DIR/.rive_tag" ]]; then
        tag="$(cat "$REPO_DIR/.rive_tag" | tr -d '\n\r')"
    fi
    # Best-effort for author/date/message when not in a git repo
    author="unknown"
    date="unknown"
    message=""
fi

# Escape strings for C quoted literals
escape_c() {
    local s="$1"
    s="${s//\\/\\\\}"
    s="${s//\"/\\\"}"
    printf '%s' "$s"
}

author_esc=$(escape_c "$author")
date_esc=$(escape_c "$date")
sha_esc=$(escape_c "$sha")
tag_esc=$(escape_c "$tag")

{
    echo "// Generated. Do not edit! See ./build_version_header.sh"
    echo
    # Emit commit message as comment lines
    # Ensure consistent line endings
    if [[ -n "${message:-}" ]]; then
        while IFS= read -r line; do
            printf '//%s\n' "$line"
        done <<<"$message"
        echo
    fi
    printf 'const char* RIVE_RUNTIME_AUTHOR="%s";\n' "$author_esc"
    printf 'const char* RIVE_RUNTIME_DATE="%s";\n' "$date_esc"
    printf 'const char* RIVE_RUNTIME_TAG="%s";\n' "$tag_esc"
    printf 'const char* RIVE_RUNTIME_SHA1="%s";\n' "$sha_esc"
} >"$OUT_PATH"

echo "Wrote $OUT_PATH"
