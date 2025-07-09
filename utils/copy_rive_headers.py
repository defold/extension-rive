#!/usr/bin/env python

import sys, os, shutil

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
    if os.path.exists(path):
        shutil.rmtree(path)

def get_version(path, pattern):
    PATTERN=pattern+'='
    with open(path, 'r') as f:
        for line in f.readlines():
            if line.startswith(PATTERN):
                tokens = line.split('=')
                return tokens[1].strip()
    return None

TARGET_DIR="./defold-rive/include/rive"

rmtree(TARGET_DIR)

RIVE_RUNTIME_VERSION = get_version('./utils/build_pls.sh', 'RIVECPP_VERSION')

copy_folder(f"./build/pls/deps/rivecpp/rive-runtime-{RIVE_RUNTIME_VERSION}/include/rive", TARGET_DIR)
copy_folder(f"./build/pls/deps/rivecpp/rive-runtime-{RIVE_RUNTIME_VERSION}/renderer/include/rive", TARGET_DIR)
copy_folder(f"./build/pls/deps/rivecpp/rive-runtime-{RIVE_RUNTIME_VERSION}/renderer/src/webgpu", os.path.join(TARGET_DIR, "renderer/webgpu"))
copy_folder(f"./build/pls/deps/rivecpp/rive-runtime-{RIVE_RUNTIME_VERSION}/renderer/glad", os.path.join(TARGET_DIR, "renderer/gl"))
copy_folder(f"./build/pls/rivecpp-tess/src/rive/tess", os.path.join(TARGET_DIR, "tess"))
copy_folder(f"./build/pls/rivecpp-tess/src/rive/math", os.path.join(TARGET_DIR, "math"))
