#!/usr/bin/env bash

SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )

# ************************************************************

if [ -z "${BOB}" ]; then
    if [ -z "${DYNAMO_HOME}" ]; then
        echo "You must specify BOB or DYNAMO_HOME!"
        exit 1
    fi
    BOB=${DYNAMO_HOME}/share/java/bob.jar
fi

echo "Using BOB=${BOB}"

if [ -z "${PYTHON}" ]; then
    PYTHON=$(which python)
fi
echo "Using PYTHON=${PYTHON}"

if [ -z "${PYTHON}" ]; then
    echo "You must have a python executable!"
    exit 1
fi

if [ ! -f "${BOB}" ]; then
    echo "Bob doesn't exist: BOB=${BOB}"
    exit 1
fi

# ************************************************************

function copyfile() {
    local path=$1
    local folder=$2
    if [ -f "$path" ]; then
        if [ ! -d "$folder" ]; then
            mkdir -v -p $folder
        fi
        cp -v $path $folder
    fi
}

function build_shader() {
    local shader_type=$1
    local platform=$2
    local vp=$3
    local fp=$4
    local dst=$5
    local variant=release
    local class_name=com.dynamo.bob.pipeline.${shader_type}

    echo "Compiling shader ${vp} for platform ${platform} with ${shader_type}"
    echo "    ${output}"

    java -cp ${BOB} ${class_name} ${vp} ${fp} ${dst} ${platform} --platform=${platform} --variant=${variant} ${OUTPUT_SPIRV}
}

function generate_cpp_source() {
    local platform=$1
    local src=$2
    local header=$3
    local source=$4

    echo "Generating source for shader ${src} for platform ${platform}"
    echo "    ${header}"
    echo "    ${source}"
    ${PYTHON} ${SCRIPT_DIR}/bin2cpp.py ${src} ${header} ${source}

    local version=$(java -jar ${BOB} --version)

    local tmpfile=_gen.tmp

    echo "// Generated with ${version}" > ${tmpfile}
    cat ${header} >> ${tmpfile}
    mv ${tmpfile} ${header}

    echo "// Generated with ${version}" > ${tmpfile}
    cat ${source} >> ${tmpfile}
    mv ${tmpfile} ${source}
}

function generate_cpp_sources() {
    local platform=$1
    local input_vp=$2
    local input_fp=$3
    local target_file=$4

    # Allow for file patterns not returning any files
    shopt -s nullglob

    echo "${input_vp} ${input_fp} ${target_file} ${platform}"

    build_shader ShaderProgramBuilder ${platform} ${input_vp} ${input_fp} ${target_file}

    local header=${target_file}.gen.h
    local source=${target_file}.gen.c

    generate_cpp_source ${platform} ${target_file} ${header} ${source}

    # enable file patterns to default behavior
    shopt -u nullglob
}
