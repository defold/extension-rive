
# Build and desploy steps

## Build Rive Runtime

*UPDATE 2025-09-20*

For most platforms we now use the build system of the rive runtime.

The build scripts do several things:
* Removes old headers
* Copy new headers
* Generates `rive_version.h`
* Removes old libraries
* Builds and copies new libraries

**NOTE** The build steps currently apply a small patch to the rive-runtiome in order to successfully build for all targets! See [rive.patch](./utils/rive.patch) for the details.

### Prerequisites

<details>
<summary>Clone the rive-runtime repo -></summary>

* Clone [rive-runtime](https://github.com/rive-app/rive-runtime) to a local folder `path/to/rive-runtime`

</details>

<details>
<summary>Prerequisites Android -></summary>

* Make sure you have ANDROID_NDK version `25.2.9519653` installed
* Make sure the ANDROID_NDK environment is set: `export ANDROID_NDK=~/Library/Android/sdk/ndk/25.2.9519653`
</details>

<details><summary>Prerequisites macOS + iOS -></summary>
Defold currently uses Xcode 15.4, so you cannot use a newer version, or the link step will fail.

* Download and install [Xcode 15.4](https://download.developer.apple.com/Developer_Tools/Xcode_15.4/Xcode_15.4.xip)
* Select it as the toolchain: `xcode-select -s /Users/name/Downloads/Xcode_15.4.app/Contents/Developer`
</details>

<details><summary>Prerequisites HTML5 -></summary>
There is no real prerequisite here, as rive-runtime can download its own copy of Emscripten.
There are two options you could override if wanted:

* Set custom Emscripten:  `export EMSDK=./path/to/emsdk-4.0.6`

* Set Emscripten to download by rive: `export RIVE_EMSDK_VERSION=4.0.6`
</details>

<details>
<summary>Prerequisites Linux -></summary>

Defold uses Ubuntu 22.04.
Do not build for a newer version, as it will lead to link errors for the users.

</details>


### Build the libraries

Build the libraries using:

```bash
cd extension-rive
./utils/build_rive_runtime.sh <platform> ./path/to/rive-runtime
```

The supported platforms are

* arm64-ios
* x86_64-ios
* arm64-macos
* x86_64-macos
* arm64-android
* armv7-android
* js-web
* wasm-web
* wasm_pthread-web
* arm64-linux
* x86_64-linux
* x86_64-win32


## CI Workflow Usage

Use the GitHub Actions workflow to build and optionally commit the generated artifacts back to the same branch.

Steps:

- Go to GitHub → Actions → "Branch Scripts and Artifacts" → Run workflow.
- Pick the target branch in the UI.
- Set inputs:
  - `platform`: choose `x86_64-linux`, `arm64-linux`, `arm64-macos`, `x86_64-macos`, `arm64-ios`, `x86_64-ios`, `arm64-android`, `armv7-android`, `js-web`, `wasm-web`, `wasm_pthread-web`, `x86_64-win32`, or `all`..
  - `rive_repo_url`: HTTPS repo for Rive runtime (defaults to `https://github.com/rive-app/rive-runtime.git`).
  - `rive_ref` (optional): pin a branch/tag of the runtime.
  - `rive_sha` (optional): pin an exact commit SHA (takes precedence over `rive_ref`).
  - `commit_message` (optional): suffix for the commit message.
  - `push_changes`: `true` to commit/push changes; `false` for a dry run.
  - `upload_artifact`: `false` by default; set `true` to upload a tarball in addition to committing changes.

What it does:

- Checks out your branch and clones the Rive runtime to a temp folder.
- Exports `RIVE_ROOT` to that folder and ensures the repo is clean.
- Runs `./utils/build_rive_runtime.sh ${PLATFORM} ${RIVE_ROOT}`.
- Uploads `branch-artifacts-<branch>.tgz` containing `defold-rive/lib` and `defold-rive/include`.
- If `push_changes=true`, commits only `defold-rive/lib` and `defold-rive/include` back to the same branch with `[skip ci]` in the message.
- If `upload_artifact=false`, the tarball upload is skipped.

Notes:

- Runners: `x86_64-linux` runs on `ubuntu-latest`; Apple targets (`arm64-macos`, `x86_64-macos`, `arm64-ios`, `x86_64-ios`) run on `macos-latest` and require Xcode command line tools.
- Android (`arm64-android`, `armv7-android`) runs on `macos-latest`; the workflow sets up the Android SDK and installs NDK r25c (25.2.9519653) via a GitHub Action, and exports `ANDROID_NDK` automatically.
  - The workflow also exports `ANDROID_NDK_HOME`, `NDK_PATH`, and sets `ANDROID_API=21` to ensure the correct sysroot is selected by Clang.
  - Note: Rive's build compiles a premake-with-ninja internally; no external premake install is required.
- Web (`js-web`, `wasm-web`, `wasm_pthread-web`) runs on `ubuntu-latest`; rive-runtime fetches Emscripten automatically unless you set `EMSDK` or `RIVE_EMSDK_VERSION`.
  - The workflow installs `glslangValidator` (package: `glslang-tools`) for shader validation during web builds.
- If you need specific SDKs (e.g., `ANDROID_NDK`, `EMSDK`), set them accordingly before invoking platform-specific builds.
- Concurrency is limited to one workflow per branch; commits include `[skip ci]` to avoid loops.


## Plugins for the editor

**NOTE** This step is also handled by the CI build step!

This step is only necessary if you've altered some particular extension code itself, namely the pluginsrc/ and commonsrc/ folders.

For a regular rive-runtime update, this step isn't needed.

Needed for the desktop platforms: macOS, Windows and Linux
For each platform+arch, do:

* Compile the rive-cpp library: `./utils/build_pls.sh <platform>`
* Build the editor plugin + jar file: `./utils/build_plugins.sh <platform>`

The `build_plugins.sh` can omit the platform flag if you wish to build all platforms at once.

You can also test the plugins locally:

    ./utils/test_plugin.sh path/to/scene.riv



