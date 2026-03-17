#!/usr/bin/env bash

set -euo pipefail

if [ $# -lt 2 ]; then
    echo "Usage: $0 <signal_dump.log> <binary>" >&2
    exit 1
fi

DUMP_FILE="$1"
BIN_FILE="$2"

if [ ! -f "${DUMP_FILE}" ]; then
    echo "Dump file not found: ${DUMP_FILE}" >&2
    exit 1
fi

if [ ! -f "${BIN_FILE}" ]; then
    echo "Binary not found: ${BIN_FILE}" >&2
    exit 1
fi

BIN_BASENAME="$(basename "${BIN_FILE}")"
BIN_REAL="$(realpath "${BIN_FILE}")"

UNAME="$(uname -s)"
if [ "${UNAME}" = "Darwin" ]; then
    if ! command -v atos >/dev/null 2>&1; then
        echo "atos not found; cannot symbolize" >&2
        exit 1
    fi
else
    if ! command -v addr2line >/dev/null 2>&1; then
        echo "addr2line not found; cannot symbolize" >&2
        exit 1
    fi
fi

echo "signal dump: ${DUMP_FILE}"
echo "binary: ${BIN_FILE}"

while IFS='|' read -r tag index addr base image symbol; do
    [ "${tag}" != "frame" ] && continue
    [ -z "${image}" ] && continue
    image_basename="$(basename "${image}")"
    image_real="$(realpath "${image}" 2>/dev/null || echo "${image}")"
    if [ "${image_basename}" != "${BIN_BASENAME}" ]; then
        continue
    fi

    if [ "${UNAME}" = "Darwin" ]; then
        if [ "${base}" != "0x0" ]; then
            line="$(atos -o "${BIN_FILE}" -l "${base}" "${addr}" | head -n 1)"
        else
            line="$(atos -o "${BIN_FILE}" "${addr}" | head -n 1)"
        fi
        line="$(echo "${line}" | sed -E 's/ \(in [^)]+\)//')"
        if [ -n "${line}" ]; then
            echo "#${index} ${line}"
        elif [ -n "${symbol}" ]; then
            echo "#${index} ${symbol}"
        fi
    else
        func=""
        fileline=""
        func="$(addr2line -f -C -e "${BIN_FILE}" "${addr}" | sed -n '1p')"
        fileline="$(addr2line -f -C -e "${BIN_FILE}" "${addr}" | sed -n '2p')"
        if [ -n "${func}" ] && [ -n "${fileline}" ]; then
            echo "#${index} ${func} (${fileline})"
        elif [ -n "${func}" ]; then
            echo "#${index} ${func}"
        elif [ -n "${symbol}" ]; then
            echo "#${index} ${symbol}"
        fi
    fi
done < "${DUMP_FILE}"
