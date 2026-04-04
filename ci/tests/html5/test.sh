#! /usr/bin/env bash

set -euo pipefail

BUNDLE_FOLDER=./bundle_web/extension-rive
REPORT_FOLDER=./build/render-test

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
  ./ci/rendertest/html5/run.sh "$@"
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

run_render_test "Grimley" \
  --mode ${MODE} \
  --bundle-dir ${BUNDLE_FOLDER} \
  --name "Grimley" \
  --collection /main/grimley/grimley.collectionc \
  --wait-mode signal \
  --expected-screenshot ./ci/tests/html5/grimley/expected.png \
  --output build/render-test/grimley \
  --likeness ${DEFAULT_LIKENESS}

run_render_test "Egg" \
  --mode ${MODE} \
  --bundle-dir ${BUNDLE_FOLDER} \
  --name "Egg" \
  --collection /main/egg/egg.collection \
  --wait-mode timeout \
  --expected-screenshot ./ci/tests/html5/egg/expected.png \
  --output build/render-test/egg \
  --likeness ${DEFAULT_LIKENESS}
