#!/usr/bin/env bash

set -e

LIBNAME=RiveExt
CLASS_NAME=com.dynamo.bob.pipeline.Rive
JAR=./defold-rive/plugins/share/plugin${LIBNAME}.jar

SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )
#pushd $SCRIPT_DIR/..

set -e

PLUGIN_PLATFORM_DIR=$(realpath ${SCRIPT_DIR}/../defold-rive/plugins/lib/x86_64-linux)
if [ "Darwin" == "$(uname)" ]; then
    if [ "arm64" == "$(arch)" ]; then
        PLUGIN_PLATFORM_DIR=$(realpath ${SCRIPT_DIR}/../defold-rive/plugins/lib/arm64-osx)
    else
        PLUGIN_PLATFORM_DIR=$(realpath ${SCRIPT_DIR}/../defold-rive/plugins/lib/x86_64-osx)
    fi
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
export DM_RIVE_ENABLE_SIGNAL_HANDLER=1

DUMP_FILE="${SCRIPT_DIR}/../build/rive_signal_dump.log"
mkdir -p "$(dirname "${DUMP_FILE}")"
rm -f "${DUMP_FILE}"
export DM_RIVE_SIGNAL_DUMP_PATH="${DUMP_FILE}"

LIB_EXT=".so"
if [ "Darwin" == "$(uname)" ]; then
    LIB_EXT=".dylib"
elif [ "MINGW" == "$(uname -s | cut -c1-5)" ]; then
    LIB_EXT=".dll"
fi
LIB_FILE="${PLUGIN_PLATFORM_DIR}/lib${LIBNAME}${LIB_EXT}"

set +e
java ${JNI_DEBUG_FLAGS} -Djava.library.path=${PLUGIN_PLATFORM_DIR} -Djni.library.path=${PLUGIN_PLATFORM_DIR} -cp ${JAR}:${BOB} ${CLASS_NAME} $*
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
