#! /usr/bin/env bash

set -euo pipefail

PLATFORM=arm64-android
PACKAGE_NAME=com.defold.extensionrive

BUNDLE_FOLDER=./bundle_android/extension-rive
REPORT_FOLDER=./build/render-tests/${PLATFORM}

DEFAULT_LIKENESS=95
MODE=direct

# TODO: Check that the game is bundled under: bundle_web/extension-rive/game.projectc
# TODO: Create the report folder

if [ -d ${REPORT_FOLDER} ]; then
  rm -rf ./${REPORT_FOLDER}
fi

mkdir -p ${REPORT_FOLDER}

run_render_test() {
  local test_name="$1"
  shift

  echo "******************************************************"
  echo "${test_name}"
  echo "******************************************************"

  set +e
  ./ci/rendertest/android/run.sh "$@"
  local exit_code=$?
  set -e

  case "${exit_code}" in
    0)
      ;;
    2)
      echo "${test_name}: render comparison failed, continuing to next test"
      ;;
    *)
      echo "${test_name}: fatal render test error, aborting test run" >&2
      exit "${exit_code}"
      ;;
  esac
}

adb shell am force-stop ${PACKAGE_NAME}

# Currently crashes!

# run_render_test "Grimley" \
#     --package ${PACKAGE_NAME} \
#     --platform ${PLATFORM} \
#     --output build/render-tests/arm64-android/grimley \
#     --collection /main/grimley/grimley.collectionc \
#     --description-file ci/tests/data/grimley/description.txt \
#     --expected-screenshot ./ci/tests/data/grimley/android/expected.png \

# adb shell am force-stop ${PACKAGE_NAME}

run_render_test "Egg" \
    --package ${PACKAGE_NAME} \
    --platform ${PLATFORM} \
    --output build/render-tests/arm64-android/egg \
    --collection /main/egg/egg.collectionc \
    --description-file ci/tests/data/egg/description.txt \
    --expected-screenshot ./ci/tests/data/egg/android/expected.png \

adb shell am force-stop ${PACKAGE_NAME}

# Currently waaaay too slow on emulation/browser!

# run_render_test "Databind" \
#     --package ${PACKAGE_NAME} \
#     --platform ${PLATFORM} \
#     --wait-ms 25000 \
#     --output build/render-tests/arm64-android/databind \
#     --collection /main/databind/databind.collectionc \
#     --description-file ci/tests/data/databind/description.txt \
#     --expected-screenshot ./ci/tests/data/databind/android/expected.png \

# adb shell am force-stop ${PACKAGE_NAME}

run_render_test "Layout" \
    --package ${PACKAGE_NAME} \
    --platform ${PLATFORM} \
    --output build/render-tests/arm64-android/layout \
    --collection /main/layout/layout.collectionc \
    --description-file ci/tests/data/layout/description.txt \
    --expected-screenshot ./ci/tests/data/layout/android/expected.png \

adb shell am force-stop ${PACKAGE_NAME}


