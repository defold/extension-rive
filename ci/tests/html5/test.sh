#! /usr/bin/env bash

set -euo pipefail

PLATFORM=wasm-web

BUNDLE_FOLDER=./bundle_web/extension-rive
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
  ./ci/rendertest/html5/run.sh --name ${test_name} "$@"
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
  --collection /main/grimley/grimley.collectionc \
  --wait-mode signal \
  --description-file ./ci/tests/data/grimley/description.txt \
  --expected-screenshot ./ci/tests/data/grimley/html5/expected.png \
  --output ${REPORT_FOLDER}/grimley \
  --likeness ${DEFAULT_LIKENESS}

run_render_test "Egg" \
  --mode ${MODE} \
  --bundle-dir ${BUNDLE_FOLDER} \
  --collection /main/egg/egg.collection \
  --wait-mode signal \
  --description-file ./ci/tests/data/egg/description.txt \
  --expected-screenshot ./ci/tests/data/egg/html5/expected.png \
  --output ${REPORT_FOLDER}/egg \
  --likeness 80

run_render_test "Layout" \
  --mode ${MODE} \
  --bundle-dir ${BUNDLE_FOLDER} \
  --collection /main/layout/layout.collection \
  --wait-mode timeout \
  --description-file ./ci/tests/data/layout/description.txt \
  --expected-screenshot ./ci/tests/data/layout/html5/expected.png \
  --output ${REPORT_FOLDER}/layout \
  --likeness ${DEFAULT_LIKENESS}


run_render_test "Out-of-band" \
  --mode ${MODE} \
  --bundle-dir ${BUNDLE_FOLDER} \
  --collection /main/outofband/outofband.collection \
  --wait-mode timeout \
  --description-file ./ci/tests/data/outofband/description.txt \
  --expected-screenshot ./ci/tests/data/outofband/html5/expected.png \
  --output ${REPORT_FOLDER}/outofband \
  --likeness ${DEFAULT_LIKENESS}


# Currently waaaay too slow on emulation!

# run_render_test "Databind" \
#   --mode ${MODE} \
#   --bundle-dir ${BUNDLE_FOLDER} \
#   --name "Databind" \
#   --collection /main/databind/databind.collection \
#   --wait-mode timeout \
#   --settle-ms 9000 \
#   --description-file ./ci/tests/data/databind/description.txt \
#   --expected-screenshot ./ci/tests/data/databind/html5/expected.png \
#   --output ${REPORT_FOLDER}/databind \
#   --likeness ${DEFAULT_LIKENESS}
