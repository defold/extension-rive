#!/usr/bin/env bash

set -euo pipefail

usage() {
    cat <<'EOF'
usage: utils/rendertest/run.sh [options]

Run a render capture against an already built web bundle.

Options:
  --mode direct|user-ui         Render test mode. Only 'direct' is implemented.
  --bundle-dir DIR              Directory containing index.html for the built bundle.
                                Default: auto-detect under ./bundle_web
  --name TEXT                   Human-readable test name shown in the report.
  --collection PATH             Collection to boot at runtime. Accepts .collection or .collectionc.
  --expected-screenshot PATH    Expected screenshot path to show in the report.
  --screenshot PATH             Output screenshot path. Relative paths are placed under the report dir.
  --likeness PERCENT           Minimum likeness percentage required to pass. Default: 95
  --output DIR                  Output directory. Default: build/render-test
  --port PORT                   Local HTTP server port. Default: 18080
  --settle-ms MS                Delay after load before capture. Default: 1500
  --timeout-ms MS               Hard timeout for the browser capture. Default: 10000
  --browser chrome|auto         Browser selection hint. Default: chrome
  --headed                      Launch browser headed for local debugging
  -h, --help                    Show this help

Environment:
  CHROME_EXECUTABLE             Optional browser executable override
EOF
}

MODE="direct"
BUNDLE_DIR=""
TEST_NAME=""
COLLECTION=""
EXPECTED_SCREENSHOT=""
SCREENSHOT_ARG=""
LIKENESS="95"
OUTPUT_DIR="build/render-test"
PORT="18080"
SETTLE_MS="500"
TIMEOUT_MS="10000"
BROWSER="chrome"
HEADED="0"

while [[ $# -gt 0 ]]; do
    case "$1" in
        --mode)
            MODE="${2:-}"
            shift 2
            ;;
        --bundle-dir)
            BUNDLE_DIR="${2:-}"
            shift 2
            ;;
        --name)
            TEST_NAME="${2:-}"
            shift 2
            ;;
        --collection)
            COLLECTION="${2:-}"
            shift 2
            ;;
        --expected-screenshot)
            EXPECTED_SCREENSHOT="${2:-}"
            shift 2
            ;;
        --screenshot)
            SCREENSHOT_ARG="${2:-}"
            shift 2
            ;;
        --likeness)
            LIKENESS="${2:-}"
            shift 2
            ;;
        --output)
            OUTPUT_DIR="${2:-}"
            shift 2
            ;;
        --port)
            PORT="${2:-}"
            shift 2
            ;;
        --settle-ms)
            SETTLE_MS="${2:-}"
            shift 2
            ;;
        --timeout-ms)
            TIMEOUT_MS="${2:-}"
            shift 2
            ;;
        --browser)
            BROWSER="${2:-}"
            shift 2
            ;;
        --headed)
            HEADED="1"
            shift
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        *)
            echo "Unknown argument: $1" >&2
            usage
            exit 1
            ;;
    esac
done

if [[ "${MODE}" != "direct" ]]; then
    echo "mode '${MODE}' is not implemented yet. Use --mode direct." >&2
    exit 1
fi

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"
OUTPUT_DIR_ABS="${REPO_ROOT}/${OUTPUT_DIR}"
REPORT_DIR="${OUTPUT_DIR_ABS}/report"
SCREENSHOT_PATH=""
RUN_JSON_PATH="${REPORT_DIR}/run.json"
RESULT_JSON_PATH="${REPORT_DIR}/result.json"
INDEX_PATH="${REPORT_DIR}/index.html"
SERVER_PID=""
RUNTIME_COLLECTION=""
LAUNCH_URL=""
EXPECTED_SCREENSHOT_ABS=""

if [[ -z "${BUNDLE_DIR}" ]]; then
    FIRST_INDEX_HTML="$(find "${REPO_ROOT}/bundle_web" -maxdepth 3 -name index.html -print -quit 2>/dev/null || true)"
    if [[ -n "${FIRST_INDEX_HTML}" ]]; then
        BUNDLE_DIR="$(dirname "${FIRST_INDEX_HTML}")"
    fi
fi

if [[ -z "${BUNDLE_DIR}" ]]; then
    echo "no bundle directory found. Pass --bundle-dir or build a bundle under ./bundle_web first." >&2
    exit 1
fi

case "${BUNDLE_DIR}" in
    /*) BUNDLE_DIR_ABS="${BUNDLE_DIR}" ;;
    *) BUNDLE_DIR_ABS="${REPO_ROOT}/${BUNDLE_DIR}" ;;
esac

if [[ ! -f "${BUNDLE_DIR_ABS}/index.html" ]]; then
    echo "index.html not found in bundle directory: ${BUNDLE_DIR_ABS}" >&2
    exit 1
fi

if [[ -n "${COLLECTION}" ]]; then
    case "${COLLECTION}" in
        /*.collection)
            RUNTIME_COLLECTION="${COLLECTION%.collection}.collectionc"
            ;;
        /*.collectionc)
            RUNTIME_COLLECTION="${COLLECTION}"
            ;;
        *)
            echo "--collection must be an absolute project path ending in .collection or .collectionc, got: ${COLLECTION}" >&2
            exit 1
            ;;
    esac
fi

if [[ -n "${EXPECTED_SCREENSHOT}" ]]; then
    case "${EXPECTED_SCREENSHOT}" in
        /*) EXPECTED_SCREENSHOT_ABS="${EXPECTED_SCREENSHOT}" ;;
        *) EXPECTED_SCREENSHOT_ABS="${REPO_ROOT}/${EXPECTED_SCREENSHOT}" ;;
    esac

    if [[ ! -f "${EXPECTED_SCREENSHOT_ABS}" ]]; then
        echo "expected screenshot not found: ${EXPECTED_SCREENSHOT_ABS}" >&2
        exit 1
    fi
fi

if [[ -n "${SCREENSHOT_ARG}" ]]; then
    case "${SCREENSHOT_ARG}" in
        /*)
            SCREENSHOT_PATH="${SCREENSHOT_ARG}"
            ;;
        *)
            SCREENSHOT_PATH="${REPORT_DIR}/${SCREENSHOT_ARG}"
            ;;
    esac
else
    SCREENSHOT_PATH="${REPORT_DIR}/screenshot.png"
fi

if ! [[ "${TIMEOUT_MS}" =~ ^[0-9]+$ ]] || [[ "${TIMEOUT_MS}" -le 0 ]]; then
    echo "--timeout-ms must be a positive integer, got: ${TIMEOUT_MS}" >&2
    exit 1
fi

if ! [[ "${LIKENESS}" =~ ^([0-9]+([.][0-9]+)?)$ ]]; then
    echo "--likeness must be a number between 0 and 100, got: ${LIKENESS}" >&2
    exit 1
fi

if ! python3 - <<PY
value = float("${LIKENESS}")
if value < 0 or value > 100:
    raise SystemExit(1)
PY
then
    echo "--likeness must be between 0 and 100, got: ${LIKENESS}" >&2
    exit 1
fi

cleanup() {
    if [[ -n "${SERVER_PID}" ]] && kill -0 "${SERVER_PID}" 2>/dev/null; then
        kill "${SERVER_PID}" 2>/dev/null || true
        wait "${SERVER_PID}" 2>/dev/null || true
    fi
}
trap cleanup EXIT

rm -rf "${REPORT_DIR}"
mkdir -p "${REPORT_DIR}"

echo "Serving bundle from ${BUNDLE_DIR_ABS} on http://127.0.0.1:${PORT}/"
python3 -m http.server "${PORT}" --bind 127.0.0.1 --directory "${BUNDLE_DIR_ABS}" > "${OUTPUT_DIR_ABS}/server.log" 2>&1 &
SERVER_PID="$!"

for _ in $(seq 1 40); do
    if python3 - <<PY
import urllib.request
urllib.request.urlopen("http://127.0.0.1:${PORT}/index.html", timeout=0.25)
PY
    then
        break
    fi
    sleep 0.25
done

if ! python3 - <<PY
import urllib.request
urllib.request.urlopen("http://127.0.0.1:${PORT}/index.html", timeout=1.0)
PY
then
    echo "local HTTP server did not become ready" >&2
    exit 1
fi

LAUNCH_URL="http://127.0.0.1:${PORT}/"
if [[ -n "${RUNTIME_COLLECTION}" ]]; then
    LAUNCH_URL="$(python3 -c 'import sys, urllib.parse; print(sys.argv[1] + "?" + urllib.parse.urlencode([("key", "bootstrap.main_collection"), ("value", sys.argv[2])]))' "${LAUNCH_URL}" "${RUNTIME_COLLECTION}")"
fi

echo "Capturing screenshot to ${SCREENSHOT_PATH}"
node "${SCRIPT_DIR}/capture.mjs" \
    --url "${LAUNCH_URL}" \
    --name "${TEST_NAME}" \
    --collection "${RUNTIME_COLLECTION}" \
    --expected-screenshot "${EXPECTED_SCREENSHOT_ABS}" \
    --browser "${BROWSER}" \
    --settle-ms "${SETTLE_MS}" \
    --timeout-ms "${TIMEOUT_MS}" \
    --likeness "${LIKENESS}" \
    --screenshot "${SCREENSHOT_PATH}" \
    --run-json "${RUN_JSON_PATH}" \
    --result-json "${RESULT_JSON_PATH}" \
    --index "${INDEX_PATH}" \
    $( [[ "${HEADED}" == "1" ]] && printf '%s' "--headed" )

echo "Render test report written to ${REPORT_DIR}"
echo "  Screenshot: ${SCREENSHOT_PATH}"
echo "  Run JSON:   ${RUN_JSON_PATH}"
echo "  Result:     ${RESULT_JSON_PATH}"
echo "  Report:     ${INDEX_PATH}"
