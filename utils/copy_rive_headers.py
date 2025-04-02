#!/usr/bin/env python

import os, shutil

def copy_file(src, tgt):
    tgtdir = os.path.dirname(tgt)
    if not os.path.exists(tgtdir):
        os.makedirs(tgtdir)

    shutil.copy2(src, tgt)
    print("Copied", src, "to", tgt)


def copy_folder(src, tgt):

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


TARGET_DIR="./defold-rive/include/rive"

rmtree(TARGET_DIR)

copy_folder("./build/pls/deps/rivecpp/rive-runtime-main/include/rive", TARGET_DIR)
copy_folder("./build/pls/deps/rivecpp/rive-runtime-main/renderer/include/rive", TARGET_DIR)
copy_folder("./build/pls/rivecpp-tess/src/rive/tess", os.path.join(TARGET_DIR, "tess"))
copy_folder("./build/pls/rivecpp-tess/src/rive/math", os.path.join(TARGET_DIR, "math"))
copy_folder("./build/pls/rivecpp-renderer/src/glad",  os.path.join(TARGET_DIR, "renderer/gl"))
