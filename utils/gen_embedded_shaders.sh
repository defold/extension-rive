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
    local src=$3
    local dst=$4
    local variant=release
    local class_name=com.dynamo.bob.pipeline.${shader_type}

    echo "Compiling shader ${src} for platform ${platform} with ${shader_type}"
    echo "    ${output}"

    java -cp ${BOB} ${class_name} ${src} ${dst} ${platform} --platform=${platform} --variant=${variant} ${OUTPUT_SPIRV}
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
    local input_dir=$2
    local target_dir=$3

    # Allow for file patterns not returning any files
    shopt -s nullglob

    for name in ${input_dir}/*.vp; do
        local short_name=$(basename $name)
        local shader=${target_dir}/${short_name}c
        build_shader VertexProgramBuilder ${platform} ${name} ${shader}
    done

    for name in ${input_dir}/*.fp; do
        local short_name=$(basename $name)
        local shader=${target_dir}/${short_name}c
        build_shader FragmentProgramBuilder ${platform} ${name} ${shader}
    done

    for name in ${target_dir}/*.vpc ${target_dir}/*.fpc; do
        local short_name=$(basename $name)
        local shader=${target_dir}/${short_name}
        local header=${target_dir}/${short_name}.gen.h
        local source=${target_dir}/${short_name}.gen.c

        generate_cpp_source ${platform} ${shader} ${header} ${source}
    done

    for name in ${input_dir}/*.gen.*; do
        echo "Generated ${name}"
    done

    # enable file patterns to default behavior
    shopt -u nullglob
}
