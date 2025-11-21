#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"

function usage() {
    echo "usage: ${BASH_SOURCE[0]} --channel CHANNEL [--version VERSION]"
    echo "If no version is provided, the version from https://d.defold.com/<channel>/info.json is used"
    exit 1
}

VERSION=""
CHANNEL="stable"
OUTPUT_SDK=""
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

if [ -z "${VERSION}" ]; then
    if ! command -v jq >/dev/null 2>&1; then
        echo "jq is required to fetch the default Defold version" >&2
        exit 1
    fi

    VERSION=$(curl -sSL https://d.defold.com/${CHANNEL}/info.json | jq -r .version)
    if [ -z "${VERSION}" ] || [ "${VERSION}" = "null" ]; then
        echo "Failed to resolve version from https://d.defold.com/stable/info.json" >&2
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

echo "Downloading bob.jar (${BOB_URL})"
curl -fSL -o "${BOB_JAR}" "${BOB_URL}"
echo "Downloaded ${BOB_JAR}"

echo "Defold SDK ready at ${OUTPUT_SDK_ROOT}; bob.jar downloaded to ${BOB_JAR}"
