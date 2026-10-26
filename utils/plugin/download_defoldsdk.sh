#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"

function usage() {
    cat <<EOF
usage: ${BASH_SOURCE[0]} --channel CHANNEL [--version VERSION] [--output-sdk DIR] [--host-platform PLATFORM] [--bob-only]
If no version is provided or "latest" is requested it reads from https://d.defold.com/<channel>/info.json.
EOF
    exit 1
}

VERSION=""
CHANNEL="stable"
OUTPUT_SDK=""
HOST_PLATFORM=""
BOB_ONLY=0
ARGS_PROVIDED=0
while [[ $# -gt 0 ]]; do
    case "$1" in
        --version)
            VERSION="${2:-}"
            shift 2
            ARGS_PROVIDED=1
            ;;
        --channel)
            CHANNEL="${2:-}"
            shift 2
            ARGS_PROVIDED=1
            ;;
        --output-sdk)
            OUTPUT_SDK="${2:-}"
            shift 2
            ARGS_PROVIDED=1
            ;;
        --host-platform)
            HOST_PLATFORM="${2:-}"
            shift 2
            ARGS_PROVIDED=1
            ;;
        --bob-only)
            BOB_ONLY=1
            shift
            ARGS_PROVIDED=1
            ;;
        -*)
            echo "Unknown option: $1" >&2
            usage
            ;;
        *)
            VERSION="$1"
            shift
            ARGS_PROVIDED=1
            ;;
    esac
done

if [[ ${ARGS_PROVIDED} -eq 0 ]]; then
    usage
fi

if [ -z "${VERSION}" ] || [ "${VERSION}" = "latest" ]; then
    if ! command -v jq >/dev/null 2>&1; then
        echo "jq is required to fetch the default Defold version" >&2
        exit 1
    fi

    VERSION=$(curl -sSL https://d.defold.com/${CHANNEL}/info.json | jq -r .version)
    if [ -z "${VERSION}" ] || [ "${VERSION}" = "null" ]; then
        echo "Failed to resolve version from https://d.defold.com/${CHANNEL}/info.json" >&2
        exit 1
    fi
fi

DOWNLOAD_BASE="https://github.com/defold/defold/releases/download/${VERSION}"
SDK_URL="${DOWNLOAD_BASE}/defoldsdk.zip"
BOB_URL="${DOWNLOAD_BASE}/bob.jar"

echo "Using Defold version: ${VERSION}"

DEFOLD_SDK_ZIP="${REPO_ROOT}/defoldsdk-${VERSION}.zip"
BOB_JAR="${REPO_ROOT}/bob.jar"

OUTPUT_SDK_ROOT="${OUTPUT_SDK:-${REPO_ROOT}/build/defoldsdk}"

if [ -f "${BOB_JAR}" ]; then
    echo "bob.jar already exists at ${BOB_JAR}, skipping download"
else
    echo "Downloading bob.jar (${BOB_URL})"
    curl -fSL -o "${BOB_JAR}" "${BOB_URL}"
    echo "Downloaded ${BOB_JAR}"
fi

if [ "${BOB_ONLY}" -eq 1 ]; then
    echo "bob-only mode: exiting after bob.jar download."
    exit 0
fi

SDK_PRESENT=0
if [ -d "${OUTPUT_SDK_ROOT}" ]; then
    echo "Defold SDK already extracted at ${OUTPUT_SDK_ROOT}, skipping download/unzip."
    SDK_PRESENT=1
else
    echo "Downloading ${SDK_URL} to ${DEFOLD_SDK_ZIP}"
    curl -fSL -o "${DEFOLD_SDK_ZIP}" "${SDK_URL}"
    echo "Downloaded ${DEFOLD_SDK_ZIP}"

    echo "Extracting ${DEFOLD_SDK_ZIP} into ${OUTPUT_SDK_ROOT}"
    mkdir -p "${OUTPUT_SDK_ROOT}"
    if command -v unzip >/dev/null 2>&1; then
        unzip -q "${DEFOLD_SDK_ZIP}" -d "${OUTPUT_SDK_ROOT}"
    else
        python - <<PY
import zipfile, pathlib
zip_path = pathlib.Path("""${DEFOLD_SDK_ZIP}""")
dest = pathlib.Path("""${OUTPUT_SDK_ROOT}""")
with zipfile.ZipFile(zip_path, 'r') as z:
    z.extractall(dest)
PY
    fi
fi

if [ ! -d "${OUTPUT_SDK_ROOT}" ]; then
    echo "Failed to extract Defold SDK to ${OUTPUT_SDK_ROOT}"
    exit 1
fi

echo "Defold SDK ready at ${OUTPUT_SDK_ROOT}"

if [ -z "${HOST_PLATFORM}" ]; then
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
fi

PROTOC_DIR="${OUTPUT_SDK_ROOT}/defoldsdk/ext/bin/${HOST_PLATFORM}"
if [ ! -f "${PROTOC_DIR}/protoc" ] && [ ! -f "${PROTOC_DIR}/protoc.exe" ]; then
    PROTOBUF_URL=$(python3 - "${VERSION}" "${HOST_PLATFORM}" <<'PY'
import json
import os
import sys
from urllib.parse import quote
from urllib.request import Request, urlopen

version, platform = sys.argv[1:]
url = f"https://api.github.com/repos/defold/defold/contents/packages?ref={quote(version, safe='')}"
headers = {"Accept": "application/vnd.github+json", "User-Agent": "extension-rive"}
token = os.environ.get("GH_TOKEN") or os.environ.get("GITHUB_TOKEN")
if token:
    headers["Authorization"] = f"Bearer {token}"
with urlopen(Request(url, headers=headers), timeout=30) as response:
    packages = json.load(response)
matches = [package["download_url"] for package in packages
           if package["name"].startswith("protobuf-")
           and package["name"].endswith(f"-{platform}.tar.gz")]
if len(matches) != 1:
    raise SystemExit(f"Expected one protobuf package for Defold {version} / {platform}, found {len(matches)}")
print(matches[0])
PY
)
    PROTOBUF_ARCHIVE="${REPO_ROOT}/build/${PROTOBUF_URL##*/}"
    PROTOBUF_DIR="${REPO_ROOT}/build/protobuf-${VERSION}-${HOST_PLATFORM}"
    mkdir -p "${PROTOBUF_DIR}" "${PROTOC_DIR}"
    echo "Downloading matching protoc from ${PROTOBUF_URL}"
    curl -fSL -o "${PROTOBUF_ARCHIVE}" "${PROTOBUF_URL}"
    tar -xzf "${PROTOBUF_ARCHIVE}" -C "${PROTOBUF_DIR}"
    PROTOC_BIN=$(find "${PROTOBUF_DIR}" -type f \( -name protoc -o -name protoc.exe \) -print -quit)
    if [ -z "${PROTOC_BIN}" ]; then
        echo "Protoc not found in ${PROTOBUF_ARCHIVE}" >&2
        exit 1
    fi
    cp "${PROTOC_BIN}" "${PROTOC_DIR}/"
    chmod +x "${PROTOC_DIR}/${PROTOC_BIN##*/}"
fi

echo "Defold protoc is available in ${PROTOC_DIR}"
