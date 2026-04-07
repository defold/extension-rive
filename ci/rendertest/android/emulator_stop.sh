#!/usr/bin/env bash

set -euo pipefail

usage() {
    cat <<'EOF'
usage: ci/rendertest/android/emulator_stop.sh [options]

Stop a running Android emulator.

Options:
  --serial SERIAL   Optional adb device serial. If omitted, a single running emulator is used.
  -h, --help        Show this help
EOF
}

SERIAL=""

while [[ $# -gt 0 ]]; do
    case "$1" in
        --serial)
            SERIAL="${2:-}"
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

ADB_BIN="${ADB_BIN:-adb}"
if ! command -v "${ADB_BIN}" >/dev/null 2>&1; then
    echo "adb not found on PATH" >&2
    exit 1
fi

if [[ -z "${SERIAL}" ]]; then
    RUNNING_SERIALS=()
    while IFS= read -r serial; do
        [[ -n "${serial}" ]] || continue
        RUNNING_SERIALS+=("${serial}")
    done < <("${ADB_BIN}" devices | awk 'NR > 1 && $2 == "device" && $1 ~ /^emulator-/ { print $1 }')

    if [[ "${#RUNNING_SERIALS[@]}" -eq 0 ]]; then
        echo "No running emulator found."
        exit 0
    fi

    if [[ "${#RUNNING_SERIALS[@]}" -gt 1 ]]; then
        printf 'Multiple running emulators found:\n' >&2
        printf '  %s\n' "${RUNNING_SERIALS[@]}" >&2
        echo "Pass --serial to choose one." >&2
        exit 1
    fi

    SERIAL="${RUNNING_SERIALS[0]}"
fi

if [[ "${SERIAL}" != emulator-* ]]; then
    echo "Serial does not look like an emulator device: ${SERIAL}" >&2
    exit 1
fi

"${ADB_BIN}" -s "${SERIAL}" emu kill >/dev/null 2>&1 || true

echo "Stopped emulator ${SERIAL}"
