#!/usr/bin/env python3

import os, sys, subprocess, shutil
import threading


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
DARWIN_PLATFORMS=['x86_64-osx', 'x86_64-macos', 'arm64-macos', 'arm64-ios', 'x86_64-ios']
HTML5_PLATFORMS=['js-web', 'wasm-web']
ANDROID_PLATFORMS=['armv7-android', 'arm64-android']
LINUX_PLATFORMS=['x86_64-linux']
SUPPORTED_PLATFORMS=WINDOWS_PLATFORMS+DARWIN_PLATFORMS+HTML5_PLATFORMS+ANDROID_PLATFORMS+LINUX_PLATFORMS

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

## **********************************************************************************************
def _to_str(x):
    if x is None:
        return ''
    elif isinstance(x, (bytes, bytearray)):
        x = str(x, encoding='utf-8')
    return x

def run_cmd(args):
    print(args)
    p = subprocess.Popen(args)
    p.wait()
    return p.returncode

def run_shell_cmd(args):
    p = subprocess.Popen(args, stdout=subprocess.PIPE)
    output = p.communicate()[0]
    output = _to_str(output).strip()
    if p.returncode != 0:
        print("Command failed:", *args)
        print(output)
        sys.exit(1)
    return output

def which(tool):
    result = run_shell_cmd(['which', tool])
    if tool not in result:
        print("No such tool found:", tool)
        sys.exit(1)
    return result

## **********************************************************************************************

MAX_THREADS=8

def async_worker(items):
    for item in items:
        try:
            fn = item[0]
            fn(*(item[1:]))
        except Exception:
            print('error with item')

def thread_work(workload):
    def divide_list(l, n):
        for i in range(0, len(l), n):
            yield l[i:i + n]

    n_threads = int(len(workload) / MAX_THREADS)
    if n_threads < 1:
        n_threads = 1

    chunk_size = int(len(workload) / n_threads)
    if chunk_size < 1:
        chunk_size = 1
    chunks = list(divide_list(workload, chunk_size))

    thread_list = []
    for chunk in divide_list(workload, chunk_size):
        thread = threading.Thread(target=async_worker, args=(chunk,))
        thread_list.append(thread)
        thread.start()

    for thread in thread_list:
        thread.join()

## **********************************************************************************************
## DARWIN

def _convert_darwin_platform(platform):
    if platform in ('x86_64-osx','x86_64-macos','arm64-macos'): return 'macosx'
    if platform in ('arm64-ios',):                              return 'iphoneos'
    if platform in ('x86_64-ios',):                             return 'iphonesimulator'
    return 'unknown'

def _get_xcode_local_path():
    return run_shell_cmd(['xcode-select','-print-path'])

# "xcode-select -print-path" will give you "/Applications/Xcode.app/Contents/Developer"
def get_local_darwin_toolchain_path():
    default_path = '%s/Toolchains/XcodeDefault.xctoolchain' % _get_xcode_local_path()
    if os.path.exists(default_path):
        return default_path
    return '/Library/Developer/CommandLineTools'

def get_local_darwin_sdk_path(platform):
    return run_shell_cmd(['xcrun','-f','--sdk', _convert_darwin_platform(platform),'--show-sdk-path']).strip()

## **********************************************************************************************
## Android

ANDROID_NDK_API_VERSION='19' # Android 4.4
ANDROID_64_NDK_API_VERSION='21' # Android 5.0

def _get_android_ndk_api_version(platform):
    if platform == 'arm64-android':
        return ANDROID_64_NDK_API_VERSION
    else:
        return ANDROID_NDK_API_VERSION

def _get_android_bin_prefix(platform):
    api_version = _get_android_ndk_api_version(platform)
    if platform == 'arm64-android':
        return 'aarch64-linux-android%s' % api_version
    else:
        return 'armv7a-linux-androideabi%s' % api_version

def get_android_clang(platform):
    prefix = _get_android_bin_prefix(platform)
    return '%s-clang' % prefix

def get_android_ranlib(platform):
    if platform == 'arm64-android':
        return 'aarch64-linux-android-ranlib'
    else:
        return 'arm-linux-androideabi-ranlib'

def get_android_ar(platform):
    if platform == 'arm64-android':
        return 'aarch64-linux-android-ar'
    else:
        return 'arm-linux-androideabi-ar'

## **********************************************************************************************
## SETUP

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
INCLUDES=[]

def verify_platform(platform):
    if platform in WINDOWS_PLATFORMS:
        expected='x86'
        if platform == 'x86_64-win32':
            expected='x64'

        arch_tgt=os.environ.get("VSCMD_ARG_TGT_ARCH", '')
        if not arch_tgt == expected:
            print("Wanted VSCMD_ARG_TGT_ARCH=%s but got" % expected, arch_tgt)
            sys.exit(1)

    if platform in DARWIN_PLATFORMS:
        pass

    if platform in HTML5_PLATFORMS:
        emscripten = os.environ.get('EMSCRIPTEN', None)
        if not emscripten:
            print("EMSCRIPTEN path is not set!")
            sys.exit(1)
        if not os.path.exists(emscripten):
            print("EMSCRIPTEN path is not a valid path!")
            sys.exit(1)

    if platform in ANDROID_PLATFORMS:
        android_ndk_home = os.environ.get('ANDROID_NDK_ROOT', None)
        if not android_ndk_home:
            print("ANDROID_NDK_ROOT path is not set!")
            sys.exit(1)

    if platform in LINUX_PLATFORMS:
        clang = which('clang') # exits upon error


def setup_vars(platform):
    global EXTENSION_LIB_FOLDER, CC, CXX, AR, RANLIB, OPT, SYSROOT, CFLAGS, CCFLAGS, CXXFLAGS, LIBSUFFIX, OBJSUFFIX, INCLUDES

    EXTENSION_LIB_FOLDER=os.path.join(EXTENSION_LIB_FOLDER, platform)

    inc = "-I"
    OPT="-Os"
    CCFLAGS=["-g"]

    if platform in WINDOWS_PLATFORMS:
        CC="cl.exe"
        CXX="cl.exe"
        RANLIB="lib.exe"
        OPT="/O2"
        CCFLAGS=[]
        CXXFLAGS=["/D_HAS_EXCEPTIONS=0", "/GR-", "/Zi", "/nologo"]
        LIBSUFFIX=".lib"
        OBJSUFFIX=".obj"
        inc = "/I"

    if platform in DARWIN_PLATFORMS:
        toolchain = get_local_darwin_toolchain_path()
        sysroot = get_local_darwin_sdk_path(platform)
        CC=os.path.join(toolchain,"/usr/bin/clang")
        CXX=os.path.join(toolchain,"/usr/bin/clang++")
        AR=os.path.join(toolchain,"/usr/bin/ar")
        RANLIB=os.path.join(toolchain,"/usr/bin/ranlib")

        CXXFLAGS=["-stdlib=libc++", "-std=c++17", "-fno-exceptions"]
        SYSROOT=['-isysroot', sysroot]

        if '-ios' in platform:
            arch = 'i386'
            if 'arm' in platform:
                arch = 'arm64'
            CCFLAGS=CCFLAGS+["-arch", arch, "-miphoneos-version-min=9.0"]

        if platform in ('x86_64-osx', 'x86_64-macos', 'arm64-macos'):
            arch = 'x86_64'
            if 'arm' in platform:
                arch = 'arm64'
            CCFLAGS=CCFLAGS+["-arch", arch, "-mmacosx-version-min=10.7"]

    if platform in HTML5_PLATFORMS:
        toolchain = os.environ.get('EMSCRIPTEN')
        CC=os.path.join(toolchain,"emcc")
        CXX=os.path.join(toolchain,"em++")
        AR=os.path.join(toolchain,"emar")
        RANLIB=os.path.join(toolchain,"emranlib")
        CCFLAGS=['-fPIC']
        CXXFLAGS=['-fno-exceptions']
        CXXFLAGS=CXXFLAGS+['-s','PRECISE_F32=2','-s','AGGRESSIVE_VARIABLE_ELIMINATION=1','-s','DISABLE_EXCEPTION_CATCHING=1']
        if platform == 'js-web':
            CXXFLAGS=CXXFLAGS+['-s','WASM=0','-s','LEGACY_VM_SUPPORT=1']
        else:
            CXXFLAGS=CXXFLAGS+['-s','WASM=1','-s','IMPORTED_MEMORY=1','-s','ALLOW_MEMORY_GROWTH=1']

    if platform in ANDROID_PLATFORMS:
        bp_os = 'linux'
        if sys.platform == 'darwin':
            bp_os = 'darwin'
        clang_name  = get_android_clang(platform)

        android_ndk_home = os.environ.get('ANDROID_NDK_ROOT', None)
        bintools    = '%s/toolchains/llvm/prebuilt/%s-x86_64/bin' % (android_ndk_home, bp_os)
        tool_name = "llvm"

        SYSROOT=['-isysroot', '%s/toolchains/llvm/prebuilt/%s-x86_64/sysroot' % (android_ndk_home, bp_os)]

        CC       = '%s/%s' % (bintools, clang_name)
        CXX      = '%s/%s++' % (bintools, clang_name)
        AR       = '%s/%s' % (bintools, get_android_ar(platform))
        RANLIB   = '%s/%s' % (bintools, get_android_ranlib(platform))

        CCFLAGS=['-fpic','-ffunction-sections','-funwind-tables','-fomit-frame-pointer','-fno-strict-aliasing','-DANDROID']
        if platform == 'armv7-android':
            CCFLAGS=CCFLAGS+['-D__ARM_ARCH_5__','-D__ARM_ARCH_5T__','-D__ARM_ARCH_5E__','-D__ARM_ARCH_5TE__','-march=armv7-a','-mfloat-abi=softfp','-mfpu=vfp','-mthumb']
        else:
            CCFLAGS=CCFLAGS+['-D__aarch64__','-march=armv8-a']
        CXXFLAGS=['-stdlib=libc++', '-std=c++17', '-fno-exceptions', '-Wno-c++11-narrowing']

    if platform in LINUX_PLATFORMS:
        CC       = which('clang')
        CXX      = which('clang++')
        AR       = which('llvm-ar')
        RANLIB   = which('llvm-ranlib')

        binpath = os.path.dirname(CC)
        sysroot = os.path.dirname(binpath)
        SYSROOT=['-isysroot', sysroot]
        CCFLAGS=['-fpic', '-fvisibility=hidden']
        CXXFLAGS=['-std=c++17', '-fno-exceptions', '-Wno-c++11-narrowing']

    assert(CC is not None)
    assert(CXX is not None)
    assert(AR is not None)
    assert(RANLIB is not None)

    INCLUDES.append(inc+"%s" % EXTENSION_INCLUDE_DIR)
    INCLUDES.append(inc+"./%s/include/mapbox" % EARCUT_UNPACK_FOLDER)
    INCLUDES.append(inc+"./%s/include" % LIBTESS2_UNPACK_FOLDER)

def find_sources(srcdir, filepattern):
    sources = []
    for root, dirs, files in os.walk(srcdir):
        for f in files:
            _,ext = os.path.splitext(f)
            if not ext or ext not in filepattern:
                continue
            sources.append(os.path.join(root, f))
    return sources

def compile_cpp_file(platform, src, tgt):
    src = os.path.normpath(src)
    tgt = os.path.normpath(tgt)

    args = [CXX, OPT] + SYSROOT + CXXFLAGS + CCFLAGS + INCLUDES
    if platform in WINDOWS_PLATFORMS:
        args = args + ["/c", src, "/Fo"+tgt]
    else:
        args = args + ["-c", src, "-o", tgt]
    run_cmd(args)

def compile_c_file(platform, src, tgt):
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

    work = []
    object_files = []
    for src in sources:
        obj = os.path.join(blddir, basename(src)) + OBJSUFFIX
        object_files.append(obj)

        work.append((compile_fn, platform, src, obj))

    thread_work(work)
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
    build_c_library(platform, "libtess2", os.path.join(LIBTESS2_UNPACK_FOLDER, "Source"), LIBTESS2_BUILD_FOLDER)

    copy_library(os.path.join(RIVE_BUILD_FOLDER, "librivecpp" + LIBSUFFIX), EXTENSION_LIB_FOLDER)
    copy_library(os.path.join(RIVE_BUILD_FOLDER, "librivetess" + LIBSUFFIX), EXTENSION_LIB_FOLDER)
    copy_library(os.path.join(LIBTESS2_BUILD_FOLDER, "libtess2" + LIBSUFFIX), EXTENSION_LIB_FOLDER)
