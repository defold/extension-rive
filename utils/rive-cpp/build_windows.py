#!/usr/bin/env python3

import os, sys, subprocess, shutil

SCRIPT_DIR=os.path.abspath(os.path.dirname(__file__))
BUILD_DIR=os.path.join(SCRIPT_DIR, "build")
EXTENSION_INCLUDE_DIR=os.path.normpath(os.path.join(SCRIPT_DIR, "..", "..", "defold-rive", "include"))
EXTENSION_LIB_FOLDER=os.path.normpath(os.path.join(SCRIPT_DIR, "..", "..", "defold-rive", "lib"))

RIVE_URL="https://github.com/rive-app/rive-cpp/archive/refs/heads/master.zip"
RIVE_UNPACK_FOLDER="build/rive-cpp-master"
RIVE_BUILD_FOLDER="rivecpp"

EARCUT_URL="https://github.com/mapbox/earcut.hpp/archive/refs/heads/master.zip"
EARCUT_UNPACK_FOLDER="build/earcut.hpp-master"

LIBTESS2_URL="https://github.com/memononen/libtess2/archive/refs/heads/master.zip"
LIBTESS2_UNPACK_FOLDER="build/libtess2-master"
LIBTESS2_BUILD_FOLDER="libtess2"

WINDOWS_PLATFORMS=['x86_64-win32', 'x86-win32']
SUPPORTED_PLATFORMS=WINDOWS_PLATFORMS

def basename(url):
    return os.path.basename(url)


def rmtree(path):
    if os.path.exists(path):
        shutil.rmtree(path)

def download_package(url, newname):
    path = os.path.join(BUILD_DIR, newname)

    if not os.path.exists(path):
        os.system("wget -O %s %s" % (path, url))
    else:
        print("Found %s, skipping." % path)

def unpack_package(name, folder):
    path = os.path.join("./build", name)
    if not os.path.exists(path):
        print("Cannot unpack! Package %s was not found!", path)
        sys.exit(1)

    if os.path.exists(folder):
        print("Folder already exists", folder)
    else:
        os.system("unzip -q %s -d build" % path)

CC=None
CXX=None
AR=None
RANLIB=None
OPT=None
LIBSUFFIX=".a"
OBJSUFFIX=".o"
SYSROOT=[]
CFLAGS=[]
CCFLAGS=[]
CXXFLAGS=[]
INCLUDES=["/I%s" % EXTENSION_INCLUDE_DIR]

def verify_platform(platform):
    if platform in WINDOWS_PLATFORMS:
        expected='x86'
        if platform == 'x86_64-win32':
            expected='x64'

        arch_tgt=os.environ.get("VSCMD_ARG_TGT_ARCH", '')
        if not arch_tgt == expected:
            print("Wanted VSCMD_ARG_TGT_ARCH=%s but got" % expected, arch_tgt)
            sys.exit(1)


def setup_vars(platform):
    global EXTENSION_LIB_FOLDER, CC, CXX, AR, RANLIB, OPT, SYSROOT, CFLAGS, CCFLAGS, CXXFLAGS, LIBSUFFIX, OBJSUFFIX, INCLUDES

    EXTENSION_LIB_FOLDER=os.path.join(EXTENSION_LIB_FOLDER, platform)

    inc = "-I"
    if platform in ('x86-win32', 'x86_64-win32'):
        CC="cl.exe"
        CXX="cl.exe"
        RANLIB="lib.exe"
        OPT="/O2"
        CXXFLAGS=["/D_HAS_EXCEPTIONS=0", "/GR-", "/Zi", "/nologo"]
        LIBSUFFIX=".lib"
        OBJSUFFIX=".obj"
        inc = "/I"

    INCLUDES.append(inc+"./%s/include/mapbox" % EARCUT_UNPACK_FOLDER)
    INCLUDES.append(inc+"./%s/include" % LIBTESS2_UNPACK_FOLDER)
        # -I./${UNPACK_FOLDER}/tess/include/ -I./${EARCUT_UNPACK_FOLDER}/include/mapbox -I./${LIBTESS2_UNPACK_FOLDER}/Include

def find_sources(srcdir, filepattern):
    sources = []
    for root, dirs, files in os.walk(srcdir):
        for f in files:
            _,ext = os.path.splitext(f)
            if not ext or ext not in filepattern:
                continue
            sources.append(os.path.join(root, f))
    return sources

def run_cmd(args):
    print(args)
    p = subprocess.Popen(args)
    p.wait()

def compile_cpp_file(platform, src, tgt):
    src = os.path.normpath(src)
    tgt = os.path.normpath(tgt)

    args = [CXX, OPT] + SYSROOT + CXXFLAGS + CCFLAGS + INCLUDES
    if platform in WINDOWS_PLATFORMS:
        args = args + ["/c", src, "/Fo"+tgt]
    else:
        args = args + ["-c", src, "-o", tgt]
    run_cmd(args)

def compile_c_file(src, tgt):
    src = os.path.normpath(src)
    tgt = os.path.normpath(tgt)

    args = [CC, OPT] + SYSROOT + CFLAGS + CCFLAGS + INCLUDES
    if platform in WINDOWS_PLATFORMS:
        args = args + ["/c", src, "/Fo"+tgt]
    else:
        args = args + ["-c", src, "-o", tgt]
    run_cmd(args)

def link_library(platform, out, object_files):
    out = os.path.normpath(out)
    if os.path.exists(out):
        os.unlink(out)

    if platform in WINDOWS_PLATFORMS:
        args = [RANLIB, "/OUT:"+out] + object_files
        run_cmd(args)
    else:
        args = [AR, "-rcs", out] + object_files
        run_cmd(args)

        if RANLIB:
            args = [RANLIB, out]
            run_cmd(args)

def build_library(platform, name, sources, blddir, compile_fn):
    if not os.path.exists(blddir):
        os.makedirs(blddir)

    object_files = []
    for src in sources:
        obj = os.path.join(blddir, basename(src)) + OBJSUFFIX
        compile_fn(platform, src, obj)
        object_files.append(obj)

    link_library(platform, os.path.join(blddir, name + LIBSUFFIX), object_files)

def build_cpp_library(platform, name, srcdir, blddir):
    sources = find_sources(srcdir, ".cpp")
    return build_library(platform, name, sources, blddir, compile_cpp_file)

def build_c_library(platform, name, srcdir, blddir):
    sources = find_sources(srcdir, ".c")
    return build_library(platform, name, sources, blddir, compile_c_file)

def copy_library(src, tgtdir):
    src = os.path.normpath(src)
    tgtdir = os.path.normpath(tgtdir)
    tgt = os.path.join(tgtdir, basename(src))
    shutil.copy2(src, tgt)
    print("Copied", src, "to", tgt)

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("You must specify a platform!", SUPPORTED_PLATFORMS)
        sys.exit(1)

    platform = sys.argv[1]
    if not platform in SUPPORTED_PLATFORMS:
        print("Platform not supported:", platform, "supported_platforms:", SUPPORTED_PLATFORMS)
        sys.exit(1)

    verify_platform(platform)
    setup_vars(platform)

    RIVE_BUILD_FOLDER = os.path.join(BUILD_DIR, platform, RIVE_BUILD_FOLDER)
    LIBTESS2_BUILD_FOLDER = os.path.join(BUILD_DIR, platform, LIBTESS2_BUILD_FOLDER)

    if not os.path.exists(RIVE_BUILD_FOLDER):
        os.makedirs(RIVE_BUILD_FOLDER)
    if not os.path.exists(LIBTESS2_BUILD_FOLDER):
        os.makedirs(LIBTESS2_BUILD_FOLDER)

    # rem copy_headers ${TARGET_INCLUDE_DIR}

    download_package(RIVE_URL, "rivecpp.zip")
    unpack_package("rivecpp.zip", RIVE_UNPACK_FOLDER)
    rmtree(os.path.join(RIVE_UNPACK_FOLDER, "tess", "src", "sokol"))
    rmtree(os.path.join(RIVE_UNPACK_FOLDER, "tess", "test"))

    download_package(EARCUT_URL, "earcut.zip")
    unpack_package("earcut.zip", EARCUT_UNPACK_FOLDER)

    download_package(LIBTESS2_URL, "libtess2.zip")
    unpack_package("libtess2.zip", LIBTESS2_UNPACK_FOLDER)

    build_cpp_library(platform, "librivecpp", os.path.join(RIVE_UNPACK_FOLDER, "src"), RIVE_BUILD_FOLDER)
    build_cpp_library(platform, "librivetess", os.path.join(RIVE_UNPACK_FOLDER, "tess"), RIVE_BUILD_FOLDER)
    build_cpp_library(platform, "libtess2", os.path.join(LIBTESS2_UNPACK_FOLDER, "Source"), LIBTESS2_BUILD_FOLDER)

    copy_library(os.path.join(RIVE_BUILD_FOLDER, "librivecpp" + LIBSUFFIX), EXTENSION_LIB_FOLDER)
    copy_library(os.path.join(RIVE_BUILD_FOLDER, "librivetess" + LIBSUFFIX), EXTENSION_LIB_FOLDER)
    copy_library(os.path.join(LIBTESS2_BUILD_FOLDER, "libtess2" + LIBSUFFIX), EXTENSION_LIB_FOLDER)
