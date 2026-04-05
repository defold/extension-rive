#!/usr/bin/env bash

set -euo pipefail

usage() {
    cat <<'EOF'
usage: ci/rendertest/android/run.sh [options]

Start an installed Android app, wait, and capture a screenshot.

Options:
  --package NAME           Android package name. Required.
  --activity NAME          Activity class name. Default: com.dynamo.android.DefoldActivity
  --collection PATH        Collection to boot at runtime. Accepts .collection or .collectionc.
  --platform TEXT          Platform name recorded in the report metadata. Default: android
  --output DIR             Output directory. Default: build/render-tests/android
  --screenshot PATH        Screenshot output path. Default: <output>/screenshot.png
  --expected-screenshot PATH   Expected screenshot path for comparison.
  --logcat-path PATH       Save logcat output to this path. Default: <report>/console.log
  --device-orientation ORIENTATION  Device orientation. Default: landscape
  --wait-ms MS             Delay before screenshot capture. Default: 1500
  --serial SERIAL          Optional adb device serial. For an emulator, use e.g. emulator-5554.
                           If omitted, a single connected device is used.
  --runtime-arg KEY=VALUE  Intent extra passed to the app. Repeatable.
  --description-file PATH  Optional description text file to copy into the output folder
  -h, --help               Show this help
EOF
}

print_command() {
    printf "Running command:"
    for arg in "$@"; do
        if [[ "$arg" == --config=* ]]; then
            printf " '%s'" "$arg"
        elif [[ "$arg" == *" "* || "$arg" == *"'"* || "$arg" == *"\""* ]]; then
            printf " '%s'" "$arg"
        else
            printf " %s" "$arg"
        fi
    done
    printf "\n"
}

PACKAGE_NAME=""
ACTIVITY_NAME="com.dynamo.android.DefoldActivity"
PLATFORM="android"
COLLECTION=""
OUTPUT_DIR="build/render-tests/android"
SCREENSHOT_ARG=""
EXPECTED_SCREENSHOT=""
CONSOLE_LOG_PATH=""
DEVICE_ORIENTATION="landscape"
WAIT_MS="1500"
SERIAL=""
DESCRIPTION_FILE=""
RUNTIME_ARGS=()

while [[ $# -gt 0 ]]; do
    case "$1" in
        --package)
            PACKAGE_NAME="${2:-}"
            shift 2
            ;;
        --activity)
            ACTIVITY_NAME="${2:-}"
            shift 2
            ;;
        --collection)
            COLLECTION="${2:-}"
            shift 2
            ;;
        --platform)
            PLATFORM="${2:-}"
            shift 2
            ;;
        --output)
            OUTPUT_DIR="${2:-}"
            shift 2
            ;;
        --screenshot)
            SCREENSHOT_ARG="${2:-}"
            shift 2
            ;;
        --expected-screenshot)
            EXPECTED_SCREENSHOT="${2:-}"
            shift 2
            ;;
        --logcat-path)
            CONSOLE_LOG_PATH="${2:-}"
            shift 2
            ;;
        --device-orientation)
            DEVICE_ORIENTATION="${2:-}"
            shift 2
            ;;
        --wait-ms)
            WAIT_MS="${2:-}"
            shift 2
            ;;
        --serial)
            SERIAL="${2:-}"
            shift 2
            ;;
        --description-file)
            DESCRIPTION_FILE="${2:-}"
            shift 2
            ;;
        --runtime-arg)
            RUNTIME_ARGS+=("${2:-}")
            shift 2
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

if ! [[ "${WAIT_MS}" =~ ^[0-9]+$ ]] || [[ "${WAIT_MS}" -lt 0 ]]; then
    echo "--wait-ms must be a non-negative integer, got: ${WAIT_MS}" >&2
    exit 1
fi

if [[ -z "${PACKAGE_NAME}" ]]; then
    echo "--package is required" >&2
    exit 1
fi

RUNTIME_COLLECTION=""
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

if [[ -n "${DESCRIPTION_FILE}" ]]; then
    if [[ ! -f "${DESCRIPTION_FILE}" ]]; then
        echo "description file not found: ${DESCRIPTION_FILE}" >&2
        exit 1
    fi
fi

TEST_NAME=""
if [[ -n "${DESCRIPTION_FILE}" ]]; then
    TEST_NAME="$(basename "$(dirname "${DESCRIPTION_FILE}")")"
    if [[ -n "${TEST_NAME}" && "$(basename "${OUTPUT_DIR}")" != "${TEST_NAME}" ]]; then
        OUTPUT_DIR="${OUTPUT_DIR%/}/${TEST_NAME}"
    fi
fi

OUTPUT_DIR_ABS="${OUTPUT_DIR}"
REPORT_DIR="${OUTPUT_DIR_ABS}/report"
SCREENSHOT_PATH="${REPORT_DIR}/screenshot.png"
RUN_JSON_PATH="${REPORT_DIR}/run.json"
RESULT_JSON_PATH="${REPORT_DIR}/result.json"
INDEX_PATH="${REPORT_DIR}/index.html"
CONSOLE_LOG_RAW_PATH="${REPORT_DIR}/logcat.raw.txt"
if [[ -z "${CONSOLE_LOG_PATH}" ]]; then
    CONSOLE_LOG_PATH="${REPORT_DIR}/console.log"
fi

case "${DEVICE_ORIENTATION}" in
    landscape|portrait)
        ;;
    *)
        echo "--device-orientation must be landscape or portrait, got: ${DEVICE_ORIENTATION}" >&2
        exit 1
        ;;
esac

if [[ -n "${SCREENSHOT_ARG}" ]]; then
    case "${SCREENSHOT_ARG}" in
        /*) SCREENSHOT_PATH="${SCREENSHOT_ARG}" ;;
        *) SCREENSHOT_PATH="${OUTPUT_DIR_ABS}/${SCREENSHOT_ARG}" ;;
    esac
fi

EXPECTED_SCREENSHOT_ABS=""
if [[ -n "${EXPECTED_SCREENSHOT}" ]]; then
    case "${EXPECTED_SCREENSHOT}" in
        /*) EXPECTED_SCREENSHOT_ABS="${EXPECTED_SCREENSHOT}" ;;
        *) EXPECTED_SCREENSHOT_ABS="${EXPECTED_SCREENSHOT}" ;;
    esac
    if [[ ! -f "${EXPECTED_SCREENSHOT_ABS}" ]]; then
        echo "expected screenshot not found: ${EXPECTED_SCREENSHOT_ABS}" >&2
        exit 1
    fi
fi

RUNTIME_EXTRA_ARGS=()
if [[ "${#RUNTIME_ARGS[@]}" -gt 0 ]]; then
    for runtime_arg in "${RUNTIME_ARGS[@]}"; do
        if [[ "${runtime_arg}" != *"="* ]]; then
            echo "--runtime-arg must be in KEY=VALUE form, got: ${runtime_arg}" >&2
            exit 1
        fi

        RUNTIME_EXTRA_ARGS+=(--es "${runtime_arg%%=*}" "${runtime_arg#*=}")
    done
fi

if [[ -n "${RUNTIME_COLLECTION}" ]]; then
    CMDLINE_VALUE="--config=bootstrap.main_collection=${RUNTIME_COLLECTION}"
    RUNTIME_EXTRA_ARGS+=(--es cmdline "${CMDLINE_VALUE}")
    RUNTIME_EXTRA_ARGS+=(--esa com.dynamo.android.EXTRA_COMMAND_LINE_ARGUMENTS "${CMDLINE_VALUE}")
fi

ADB_BIN="${ADB_BIN:-adb}"
if ! command -v "${ADB_BIN}" >/dev/null 2>&1; then
    echo "adb not found on PATH" >&2
    exit 1
fi

LOGCAT_CAPTURE_PID=""
APP_PID=""

cleanup_logcat() {
    if [[ -n "${LOGCAT_CAPTURE_PID}" ]]; then
        kill "${LOGCAT_CAPTURE_PID}" >/dev/null 2>&1 || true
        wait "${LOGCAT_CAPTURE_PID}" >/dev/null 2>&1 || true
        LOGCAT_CAPTURE_PID=""
    fi
}

trap cleanup_logcat EXIT

start_logcat_capture() {
    rm -f "${CONSOLE_LOG_RAW_PATH}"
    "${ADB_BIN}" "${ADB_DEVICE_ARGS[@]}" logcat -v threadtime > "${CONSOLE_LOG_RAW_PATH}" 2>&1 &
    LOGCAT_CAPTURE_PID="$!"
}

find_app_pid() {
    local attempt pid
    for attempt in $(seq 1 60); do
        pid="$("${ADB_BIN}" "${ADB_DEVICE_ARGS[@]}" shell pidof -s "${PACKAGE_NAME}" 2>/dev/null | tr -d '\r' | awk 'NF { print $1; exit }')"
        if [[ -n "${pid}" ]]; then
            printf '%s\n' "${pid}"
            return 0
        fi
        sleep 0.5
    done

    return 1
}

apply_device_orientation() {
    local user_rotation
    case "${DEVICE_ORIENTATION}" in
        landscape)
            user_rotation="1"
            ;;
        portrait)
            user_rotation="0"
            ;;
    esac

    "${ADB_BIN}" "${ADB_DEVICE_ARGS[@]}" shell settings put system accelerometer_rotation 0
    "${ADB_BIN}" "${ADB_DEVICE_ARGS[@]}" shell settings put system user_rotation "${user_rotation}"
}

ADB_DEVICE_SERIAL="${SERIAL}"
if [[ -z "${ADB_DEVICE_SERIAL}" ]]; then
    CONNECTED_SERIALS=()
    while IFS= read -r serial; do
        [[ -n "${serial}" ]] || continue
        CONNECTED_SERIALS+=("${serial}")
    done < <("${ADB_BIN}" devices | awk 'NR > 1 && $2 == "device" { print $1 }')

    if [[ "${#CONNECTED_SERIALS[@]}" -eq 0 ]]; then
        echo "No connected Android devices found. Pass --serial to target a specific device." >&2
        exit 1
    fi

    if [[ "${#CONNECTED_SERIALS[@]}" -gt 1 ]]; then
        printf 'Multiple connected Android devices found:\n' >&2
        printf '  %s\n' "${CONNECTED_SERIALS[@]}" >&2
        echo "Pass --serial to choose one." >&2
        exit 1
    fi

    ADB_DEVICE_SERIAL="${CONNECTED_SERIALS[0]}"
fi

ADB_DEVICE_ARGS=(-s "${ADB_DEVICE_SERIAL}")

mkdir -p "${OUTPUT_DIR_ABS}"
mkdir -p "${REPORT_DIR}"

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

"${ADB_BIN}" "${ADB_DEVICE_ARGS[@]}" wait-for-device
apply_device_orientation
"${ADB_BIN}" "${ADB_DEVICE_ARGS[@]}" shell am force-stop "${PACKAGE_NAME}" >/dev/null 2>&1 || true
start_logcat_capture
AM_START_CMD=(
    "${ADB_BIN}"
    "${ADB_DEVICE_ARGS[@]}"
    shell
    am
    start
    -W
    -n "${PACKAGE_NAME}/${ACTIVITY_NAME}"
    -a android.intent.action.MAIN
    -c android.intent.category.LAUNCHER
)
if [[ "${#RUNTIME_EXTRA_ARGS[@]}" -gt 0 ]]; then
    AM_START_CMD+=("${RUNTIME_EXTRA_ARGS[@]}")
fi
print_command "${AM_START_CMD[@]}"
"${AM_START_CMD[@]}"

if APP_PID="$(find_app_pid)"; then
    echo "Captured app PID ${APP_PID}"
else
    echo "Warning: failed to resolve PID for ${PACKAGE_NAME}; logcat will be saved unfiltered" >&2
fi

sleep_seconds="$(printf '%d.%03d' "$((WAIT_MS / 1000))" "$((WAIT_MS % 1000))")"
sleep "${sleep_seconds}"

"${ADB_BIN}" "${ADB_DEVICE_ARGS[@]}" exec-out screencap -p > "${SCREENSHOT_PATH}"

cleanup_logcat

if [[ -n "${APP_PID}" && -f "${CONSOLE_LOG_RAW_PATH}" ]]; then
    awk -v pid="${APP_PID}" '($3 == pid)' "${CONSOLE_LOG_RAW_PATH}" > "${CONSOLE_LOG_PATH}"
else
    cp "${CONSOLE_LOG_RAW_PATH}" "${CONSOLE_LOG_PATH}"
fi

if [[ -n "${DESCRIPTION_FILE}" ]]; then
    cp "${DESCRIPTION_FILE}" "${OUTPUT_DIR_ABS}/description.txt"
fi

export RUN_JSON_PATH
export PACKAGE_NAME
export ACTIVITY_NAME
export PLATFORM
export OUTPUT_DIR_ABS
export SCREENSHOT_PATH
export EXPECTED_SCREENSHOT_ABS
export WAIT_MS
export SERIAL
export DESCRIPTION_FILE
export ADB_DEVICE_SERIAL
export TEST_NAME
export APP_PID
export CONSOLE_LOG_PATH
export CONSOLE_LOG_RAW_PATH
export DEVICE_ORIENTATION

python3 -B - <<'PY'
import json
import os
from pathlib import Path
from datetime import datetime, timezone

def read_description(description_path):
    if not description_path:
        return ""
    text = Path(description_path).read_text(encoding="utf8")
    return text.replace("\r", "").rstrip()

run_json_path = Path(os.environ["RUN_JSON_PATH"])
run_data = {
    "mode": "android",
    "package_name": os.environ["PACKAGE_NAME"],
    "activity_name": os.environ["ACTIVITY_NAME"],
    "platform": os.environ["PLATFORM"],
    "test_group": os.environ.get("TEST_NAME") or Path(os.environ["OUTPUT_DIR_ABS"]).name,
    "description": read_description(os.environ.get("DESCRIPTION_FILE", "")),
    "screenshot_path": os.environ["SCREENSHOT_PATH"],
    "wait_ms": int(os.environ["WAIT_MS"]),
    "captured_at": datetime.now(timezone.utc).replace(microsecond=0).isoformat().replace("+00:00", "Z"),
    "device_serial": os.environ.get("ADB_DEVICE_SERIAL", ""),
    "app_pid": os.environ.get("APP_PID", ""),
    "console_log_path": os.environ.get("CONSOLE_LOG_PATH", ""),
    "console_log_raw_path": os.environ.get("CONSOLE_LOG_RAW_PATH", ""),
    "device_orientation": os.environ.get("DEVICE_ORIENTATION", ""),
}
run_json_path.write_text(json.dumps(run_data, indent=2) + "\n", encoding="utf8")
PY

BUILD_REPORT_ARGS=(
    python3
    "${SCRIPT_DIR}/../build_report.py"
    --run-json "${RUN_JSON_PATH}"
    --result-json "${RESULT_JSON_PATH}"
    --index "${INDEX_PATH}"
)

if [[ -n "${EXPECTED_SCREENSHOT_ABS}" ]]; then
    BUILD_REPORT_ARGS+=(--expected-screenshot "${EXPECTED_SCREENSHOT_ABS}")
fi

"${BUILD_REPORT_ARGS[@]}"

echo "Android screenshot written to ${SCREENSHOT_PATH}"
echo "Run JSON: ${RUN_JSON_PATH}"
echo "Result:   ${RESULT_JSON_PATH}"
echo "Report:   ${INDEX_PATH}"
