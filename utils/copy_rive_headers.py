#!/usr/bin/env python

import sys, os, shutil
import write_rive_version_header

def copy_file(src, tgt):
    tgtdir = os.path.dirname(tgt)
    if not os.path.exists(tgtdir):
        os.makedirs(tgtdir)

    shutil.copy2(src, tgt)
    print("Copied", src, "to", tgt)


def copy_folder(src, tgt):
    if not os.path.exists(src):
        print(f"Path does not exist: {src}")
        sys.exit(1)

    for root, dirs, files in os.walk(src):
        for f in files:
            header = f.endswith(".h") or f.endswith(".hpp")
            if not header:
                continue

            fullsrc = os.path.join(root, f)
            relativesrc = os.path.relpath(fullsrc, src)
            fulltgt = os.path.join(tgt, relativesrc)

            copy_file(fullsrc, fulltgt)

def rmtree(path):
    print("Removing", path)
    if os.path.exists(path):
        shutil.rmtree(path)

def get_version(path, pattern):
    PATTERN=pattern+'='
    assert(os.path.exists(path))
    with open(path, 'r') as f:
        for line in f.readlines():
            line = line.strip()
            if line.startswith(PATTERN):
                tokens = line.split('=')
                return tokens[1].strip()
    return None

def generate_rive_version_header(path, sha1):
    info = write_rive_version_header.GetCommitDetails(sha1)
    write_rive_version_header.WriteHeader(path, info)

DEFAULT_INCLUDE_DIR="./defold-rive/include"
TARGET_DIR="./defold-rive/include/rive"
DEFOLD_TARGET_DIR="./defold-rive/include/defold"

print("******************************")

rmtree(TARGET_DIR)
rmtree(os.path.join(DEFAULT_INCLUDE_DIR, "glad"))
rmtree(os.path.join(DEFAULT_INCLUDE_DIR, "KHR"))

print("******************************")

RIVE_RUNTIME_VERSION = get_version('./utils/build_pls.sh', 'RIVECPP_VERSION')
assert(RIVE_RUNTIME_VERSION is not None)

copy_folder(f"./build/pls/deps/rivecpp/rive-runtime-{RIVE_RUNTIME_VERSION}/include/rive", TARGET_DIR)
copy_folder(f"./build/pls/deps/rivecpp/rive-runtime-{RIVE_RUNTIME_VERSION}/renderer/include/rive", TARGET_DIR)
copy_folder(f"./build/pls/deps/rivecpp/rive-runtime-{RIVE_RUNTIME_VERSION}/renderer/src/webgpu", os.path.join(TARGET_DIR, "renderer/webgpu"))
copy_folder(f"./build/pls/deps/rivecpp/rive-runtime-{RIVE_RUNTIME_VERSION}/renderer/glad", os.path.join(TARGET_DIR, "renderer/gl"))
copy_folder(f"./build/pls/deps/rivecpp/rive-runtime-{RIVE_RUNTIME_VERSION}/renderer/glad/include/glad", os.path.join(DEFAULT_INCLUDE_DIR, "glad"))
copy_folder(f"./build/pls/deps/rivecpp/rive-runtime-{RIVE_RUNTIME_VERSION}/renderer/glad/include/KHR", os.path.join(DEFAULT_INCLUDE_DIR, "KHR"))
copy_folder(f"./build/pls/rivecpp-tess/src/rive/tess", os.path.join(TARGET_DIR, "tess"))
copy_folder(f"./build/pls/rivecpp-tess/src/rive/math", os.path.join(TARGET_DIR, "math"))

# As they don't have a version system, we need to provide our own
generate_rive_version_header(os.path.join(DEFOLD_TARGET_DIR, 'rive_version.h'), RIVE_RUNTIME_VERSION)

