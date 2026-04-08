#!/usr/bin/env bash

set -e

LIBNAME=RiveExt
CLASS_NAME=com.dynamo.bob.pipeline.Rive

SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )
REPO_ROOT=$(realpath "${SCRIPT_DIR}/../..")
JAR="${REPO_ROOT}/defold-rive/plugins/share/plugin${LIBNAME}.jar"

set -e

UNAME_S="$(uname -s)"
UNAME_M="$(uname -m)"
IS_WINDOWS=0
PLUGIN_PLATFORM="x86_64-linux"
case "${UNAME_S}" in
    Darwin)
        if [ "arm64" == "${UNAME_M}" ]; then
            PLUGIN_PLATFORM="arm64-osx"
        else
            PLUGIN_PLATFORM="x86_64-osx"
        fi
        ;;
    MINGW*|MSYS*|CYGWIN*|Windows_NT)
        PLUGIN_PLATFORM="x86_64-win32"
        IS_WINDOWS=1
        ;;
    *)
        if [ "arm64" == "${UNAME_M}" ] || [ "aarch64" == "${UNAME_M}" ]; then
            PLUGIN_PLATFORM="arm64-linux"
        else
            PLUGIN_PLATFORM="x86_64-linux"
        fi
        ;;
esac
PLUGIN_PLATFORM_DIR=$(realpath "${REPO_ROOT}/defold-rive/plugins/lib/${PLUGIN_PLATFORM}")
LOCAL_PLUGIN_PLATFORM_DIR="${SCRIPT_DIR}/build/${PLUGIN_PLATFORM}"

if [ -d "${LOCAL_PLUGIN_PLATFORM_DIR}" ]; then
    PLUGIN_PLATFORM_DIR="$(realpath "${LOCAL_PLUGIN_PLATFORM_DIR}")"
fi

if [ -z "${JAR}" ]; then
    echo "Couldn't find the jar file!"
fi

if [ "" == "${BOB}" ]; then
    BOB=~/work/defold/tmp/dynamo_home/share/java/bob.jar
fi

echo "BOB=${BOB}"
echo "JAR=${JAR}"
echo "CLASS_NAME=${CLASS_NAME}"
echo "LIBNAME=${LIBNAME}"
#echo "PLUGIN_PLATFORM_DIR=${PLUGIN_PLATFORM_DIR}"

JNI_DEBUG_FLAGS="-Xcheck:jni"

export DM_RIVE_LOG_LEVEL=DEBUG
#export DM_RIVE_ENABLE_SIGNAL_HANDLER=1

DYN_PRELOAD_VAR=""
JSIG_PATH=""
if [ -n "${JAVA_HOME:-}" ]; then
    if [ "Darwin" == "$(uname)" ]; then
        if [ -f "${JAVA_HOME}/lib/libjsig.dylib" ]; then
            JSIG_PATH="${JAVA_HOME}/lib/libjsig.dylib"
        elif [ -f "${JAVA_HOME}/jre/lib/libjsig.dylib" ]; then
            JSIG_PATH="${JAVA_HOME}/jre/lib/libjsig.dylib"
        fi
        DYN_PRELOAD_VAR="DYLD_INSERT_LIBRARIES"
    else
        if [ -f "${JAVA_HOME}/lib/libjsig.so" ]; then
            JSIG_PATH="${JAVA_HOME}/lib/libjsig.so"
        elif [ -f "${JAVA_HOME}/jre/lib/aarch64/libjsig.so" ]; then
            JSIG_PATH="${JAVA_HOME}/jre/lib/aarch64/libjsig.so"
        elif [ -f "${JAVA_HOME}/jre/lib/amd64/libjsig.so" ]; then
            JSIG_PATH="${JAVA_HOME}/jre/lib/amd64/libjsig.so"
        elif [ -f "${JAVA_HOME}/jre/lib/x86_64/libjsig.so" ]; then
            JSIG_PATH="${JAVA_HOME}/jre/lib/x86_64/libjsig.so"
        fi
        DYN_PRELOAD_VAR="LD_PRELOAD"
    fi
fi

if [ -n "${JSIG_PATH}" ]; then
    if [ "${DYN_PRELOAD_VAR}" = "DYLD_INSERT_LIBRARIES" ]; then
        export DYLD_FORCE_FLAT_NAMESPACE=1
        export DYLD_INSERT_LIBRARIES="${JSIG_PATH}${DYLD_INSERT_LIBRARIES:+:${DYLD_INSERT_LIBRARIES}}"
    else
        export LD_PRELOAD="${JSIG_PATH}${LD_PRELOAD:+:${LD_PRELOAD}}"
    fi
else
    echo "libjsig not found; skipping preload"
fi

DUMP_FILE="${REPO_ROOT}/build/rive_signal_dump.log"
mkdir -p "$(dirname "${DUMP_FILE}")"
rm -f "${DUMP_FILE}"
export DM_RIVE_SIGNAL_DUMP_PATH="${DUMP_FILE}"

LIB_EXT=".so"
if [ "Darwin" == "${UNAME_S}" ]; then
    LIB_EXT=".dylib"
elif [ "${IS_WINDOWS}" -eq 1 ]; then
    LIB_EXT=".dll"
fi
LIB_FILE="${PLUGIN_PLATFORM_DIR}/lib${LIBNAME}${LIB_EXT}"

JAVA_PLUGIN_PLATFORM_DIR="${PLUGIN_PLATFORM_DIR}"
JAVA_JAR="${JAR}"
JAVA_BOB="${BOB}"
CLASSPATH_SEP=":"
if [ "${IS_WINDOWS}" -eq 1 ]; then
    CLASSPATH_SEP=";"
    if command -v cygpath >/dev/null 2>&1; then
        JAVA_PLUGIN_PLATFORM_DIR="$(cygpath -m "${PLUGIN_PLATFORM_DIR}")"
        JAVA_JAR="$(cygpath -m "${JAR}")"
        JAVA_BOB="$(cygpath -m "${BOB}")"
    fi
fi
JAVA_CLASSPATH="${JAVA_JAR}${CLASSPATH_SEP}${JAVA_BOB}"

set +e
if [ "${IS_WINDOWS}" -eq 1 ]; then
    MSYS2_ARG_CONV_EXCL="*" MSYS_NO_PATHCONV=1 \
    java ${JNI_DEBUG_FLAGS} -Djava.library.path="${JAVA_PLUGIN_PLATFORM_DIR}" -Djni.library.path="${JAVA_PLUGIN_PLATFORM_DIR}" -cp "${JAVA_CLASSPATH}" ${CLASS_NAME} "$@"
else
    java ${JNI_DEBUG_FLAGS} -Djava.library.path="${JAVA_PLUGIN_PLATFORM_DIR}" -Djni.library.path="${JAVA_PLUGIN_PLATFORM_DIR}" -cp "${JAVA_CLASSPATH}" ${CLASS_NAME} "$@"
fi
JAVA_STATUS=$?
set -e

if [ "${JAVA_STATUS}" -ne 0 ]; then
    if [ -f "${DUMP_FILE}" ]; then
        printf "\n"
        printf "\n"
        printf "\n"
        echo "***************************************************************************************"
        "${SCRIPT_DIR}/symbolize_signal_dump.sh" "${DUMP_FILE}" "${LIB_FILE}"

        printf "\n"
        printf "\n"
    else
        echo "signal dump not found at ${DUMP_FILE}"
    fi
fi

exit ${JAVA_STATUS}
