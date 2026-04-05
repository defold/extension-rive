#!/usr/bin/env bash

set -euo pipefail

usage() {
    cat <<'EOF'
usage: ci/rendertest/android/install.sh --apk PATH [options]

Install or reinstall an Android APK on a connected device.

Options:
  --apk PATH        APK input path to reinstall. Required.
  --serial SERIAL   Optional adb device serial. If omitted, a single connected device is used.
  -h, --help        Show this help
EOF
}

APK_PATH=""
SERIAL=""

while [[ $# -gt 0 ]]; do
    case "$1" in
        --apk)
            APK_PATH="${2:-}"
            shift 2
            ;;
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

if [[ -z "${APK_PATH}" ]]; then
    echo "--apk is required" >&2
    exit 1
fi

if [[ ! -f "${APK_PATH}" ]]; then
    echo "APK not found: ${APK_PATH}" >&2
    exit 1
fi

ADB_BIN="${ADB_BIN:-adb}"
if ! command -v "${ADB_BIN}" >/dev/null 2>&1; then
    echo "adb not found on PATH" >&2
    exit 1
fi

find_aapt_binary() {
    local sdk_dir candidate
    local sdk_dirs=(
        "${ANDROID_SDK_ROOT:-}"
        "${ANDROID_HOME:-}"
        "${HOME}/Android/Sdk"
        "${HOME}/Library/Android/sdk"
        "${LOCALAPPDATA:-}/Android/Sdk"
    )

    if command -v aapt >/dev/null 2>&1; then
        printf '%s\n' "aapt"
        return 0
    fi

    for sdk_dir in "${sdk_dirs[@]}"; do
        [[ -n "${sdk_dir}" && -d "${sdk_dir}/build-tools" ]] || continue
        while IFS= read -r candidate; do
            [[ -x "${candidate}" ]] || continue
            printf '%s\n' "${candidate}"
            return 0
        done < <(find "${sdk_dir}/build-tools" -maxdepth 2 \( -name aapt -o -name aapt.exe \) 2>/dev/null | sort)
    done

    return 1
}

detect_package_name() {
    local aapt_bin
    if ! aapt_bin="$(find_aapt_binary)"; then
        echo "aapt not found on PATH or in common Android SDK locations. Set ANDROID_SDK_ROOT or add build-tools to PATH." >&2
        return 1
    fi

    "${aapt_bin}" dump badging "${APK_PATH}" 2>/dev/null | awk -F"'" '/^package: name=/ { print $2; exit }'
}

PACKAGE_NAME="$(detect_package_name)"
if [[ -z "${PACKAGE_NAME}" ]]; then
    echo "Failed to determine package name from APK: ${APK_PATH}" >&2
    exit 1
fi

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
        echo "Pass --serial to choose one, for example --serial emulator-5554." >&2
        exit 1
    fi

    ADB_DEVICE_SERIAL="${CONNECTED_SERIALS[0]}"
fi

ADB_DEVICE_ARGS=(-s "${ADB_DEVICE_SERIAL}")

"${ADB_BIN}" "${ADB_DEVICE_ARGS[@]}" wait-for-device
"${ADB_BIN}" "${ADB_DEVICE_ARGS[@]}" install -r -t -g "${APK_PATH}"

echo "Installed ${APK_PATH} on ${ADB_DEVICE_SERIAL}"
printf 'PACKAGE_NAME=%s\n' "${PACKAGE_NAME}"
