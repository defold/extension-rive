# Viewer Build Notes

## Prerequisites

- `DYNAMO_HOME` must point to your Defold SDK root (the folder containing `include/`, `lib/`, and `ext/`).
- `cmake` and `ninja` must be installed and on `PATH`.
- `python3` must be available on `PATH` (used for shader generation).
- `BOB` is optional. If unset, CMake uses `${DYNAMO_HOME}/share/java/bob.jar`.

## Supported Target Platforms

- `arm64-macos`
- `x86_64-win32`
- `x86_64-linux`
- `arm64-linux`

## Quick Build (wrapper script)

From repo root:

```bash
./utils/viewer/build.sh x86_64-win32
```

Temporarily force Windows Rive libs from `utils/libs_win64`:

```bash
./utils/viewer/build.sh --use-utils-libs-win64 x86_64-win32
```

Or point to any custom Windows lib directory:

```bash
./utils/viewer/build.sh --rive-lib-dir /c/repos/extension-rive/utils/libs_win64 x86_64-win32
```

Custom config / ASAN:

```bash
./utils/viewer/build.sh --config RelWithDebInfo --with-asan x86_64-linux
```

## Build With CMake Directly

From repo root (Linux/macOS example):

```bash
cmake -S utils/viewer -B utils/viewer/build/x86_64-linux -G Ninja \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DTARGET_PLATFORM=x86_64-linux

cmake --build utils/viewer/build/x86_64-linux --target viewer --config RelWithDebInfo
```

Output binary:

- Windows: `utils/viewer/build/x86_64-win32/viewer.exe`
- Linux/macOS: `utils/viewer/build/<target>/viewer`

Headless fallback test:

```bash
./utils/viewer/build/x86_64-linux/viewer --headless ./main/grimley/4951-10031-grimley.riv
```

## Create Visual Studio Solution (Windows)

Shortcut script:

```bash
./utils/viewer/make_solution.sh
```

Optional flags:

- `--config <RelWithDebInfo|Release|Debug|MinSizeRel>` (default: `RelWithDebInfo`)
- `--build` (build `viewer` after generating solution)
- `--no-build` (generate solution only; default behavior)
- `--build-dir <path>` (override output directory)
- `--use-utils-libs-win64` (force `utils/libs_win64` for Windows Rive libs)
- `--rive-lib-dir <path>` (force a specific Windows Rive lib directory)

```powershell
# Run from repo root (Developer PowerShell recommended)
$env:DYNAMO_HOME = "C:/repos/defold/tmp/dynamo_home"
$env:BOB = "$env:DYNAMO_HOME/share/java/bob.jar"   # optional

cmake -S utils/viewer -B utils/viewer/build/x86_64-win32 `
  -G "Visual Studio 17 2022" `
  -A x64 `
  -DTARGET_PLATFORM=x86_64-win32

cmake --build utils/viewer/build/x86_64-win32 --target viewer --config RelWithDebInfo
```

Open the solution:

- `utils/viewer/build/x86_64-win32/viewer.sln`

## Windows Toolchain Note

- For `x86_64-win32`, build with MSVC/Visual Studio tools (not MinGW/MSYS `g++`).
- `./utils/viewer/build.sh x86_64-win32` now auto-selects `Visual Studio 17 2022` (`-A x64`) when `CMAKE_GENERATOR` is not explicitly set.
- `Debug` config is not supported for `x86_64-win32` with current prebuilt `defold-rive` libs. Use `RelWithDebInfo` (default) or `Release`.
- On `x86_64-win32`, CMake now auto-prefers `utils/libs_win64` when that folder exists; otherwise it falls back to `defold-rive/lib/x86_64-win32`.
- You can override this via `-DVIEWER_WIN32_RIVE_LIB_DIR=<path>` when invoking CMake directly.
