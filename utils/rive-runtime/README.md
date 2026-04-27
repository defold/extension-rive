# Rive Runtime Build

Build and deployment steps for the prebuilt Rive runtime libraries used by this repo.

## Overview

We use the upstream Rive runtime build system to build most libraries, then copy headers and artifacts into `defold-rive/include` and `defold-rive/lib`.

The build script does several things:

- Removes old headers
- Copies new headers
- Generates `rive_version.h`
- Removes old libraries
- Builds and copies new libraries

The build steps currently apply a small patch to the Rive runtime in order to successfully build for all targets. See [rive.patch](/Users/mathiaswesterdahl/work/projects/users/defold/extension-rive/utils/rive-runtime/rive.patch).

## Build Locally

Build the libraries using:

```bash
cd extension-rive
./utils/rive-runtime/build.sh <platform> ./path/to/rive-runtime
```

Supported platforms:

- `arm64-ios`
- `x86_64-ios`
- `arm64-macos`
- `x86_64-macos`
- `arm64-android`
- `armv7-android`
- `js-web`
- `wasm-web`
- `wasm_pthread-web`
- `arm64-linux`
- `x86_64-linux`
- `x86_64-win32`

## Prerequisites

### Clone the Runtime

- Clone [rive-runtime](https://github.com/rive-app/rive-runtime) to a local folder such as `path/to/rive-runtime`

### Android

- Install Android NDK `25.2.9519653`
- Set `ANDROID_NDK`, for example:
  `export ANDROID_NDK=~/Library/Android/sdk/ndk/25.2.9519653`

### macOS and iOS

Defold currently uses Xcode 15.4. Using a newer version can break the link step.

- Install [Xcode 15.4](https://download.developer.apple.com/Developer_Tools/Xcode_15.4/Xcode_15.4.xip)
- Select it:
  `xcode-select -s /Users/name/Downloads/Xcode_15.4.app/Contents/Developer`

### HTML5

Rive can download its own Emscripten toolchain. Optional overrides:

- `export EMSDK=./path/to/emsdk-4.0.6`
- `export RIVE_EMSDK_VERSION=4.0.6`

### Linux

Defold uses Ubuntu 22.04. Do not build against a newer distro if you want compatible binaries.

## GitHub Actions

Using GitHub Actions is the easiest way to update the runtime libraries and related artifacts.

### Create a Branch

Use the runtime SHA in the branch name for clarity:

```bash
git checkout -b update-<sha1>
```

### Run the Workflow

Example with GitHub CLI:

```bash
gh workflow run branch-artifacts.yml -r update-<sha1> -f platform=all -f push_changes=true -f rive_sha=<sha1>
```

Example using a Rive runtime release tag:

```bash
gh workflow run branch-artifacts.yml -r update-runtime-v0.1.8 -f platform=all -f push_changes=true -f rive_tag=runtime-v0.1.8
```

Example for a single platform:

```bash
gh workflow run branch-artifacts.yml -r update-a228887fa6032efd0e0e23af70455913dee4ac1f -f platform=x86_64-win32 -f push_changes=true -f rive_sha=a228887fa6032efd0e0e23af70455913dee4ac1f
```

### Workflow Behavior

The workflow:

- Checks out your branch and clones the Rive runtime to a temp folder
- Checks out the requested `rive_sha`, `rive_tag`, or `rive_ref`; SHA takes precedence over tag, and tag takes precedence over ref
- Exports `RIVE_ROOT` to that folder and ensures the repo is clean
- Runs `./utils/rive-runtime/build.sh ${PLATFORM} ${RIVE_ROOT}`
- Uploads `branch-artifacts-<branch>.tgz` containing `defold-rive/lib` and `defold-rive/include`
- If `push_changes=true`, commits `defold-rive/lib` and `defold-rive/include` back to the same branch with `[skip ci]`
- If `upload_artifact=false`, skips the tarball upload

Workflow notes:

- `x86_64-linux` runs on `ubuntu-latest`
- Apple targets run on `macos-latest` and require Xcode command line tools
- Android runs on `macos-latest` and installs NDK r25c automatically
- Web builds install `glslangValidator` for shader validation
- Concurrency is limited to one workflow per branch
