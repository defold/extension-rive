#! /usr/bin/env bash


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

echo "******************************************************"
echo "Grimley"
echo "******************************************************"

./ci/rendertest/run.sh \
  --mode ${MODE} \
  --bundle-dir ${BUNDLE_FOLDER} \
  --name "Grimley" \
  --collection /main/grimley/grimley.collectionc \
  --expected-screenshot ./ci/tests/html5/expected/grimley.png \
  --screenshot grimley.png \
  --output build/render-test/grimley \
  --likeness ${DEFAULT_LIKENESS}

echo "******************************************************"
echo "Egg"
echo "******************************************************"

./ci/rendertest/run.sh \
  --mode ${MODE} \
  --bundle-dir ${BUNDLE_FOLDER} \
  --name "Egg" \
  --collection /main/egg/egg.collection \
  --expected-screenshot ./ci/tests/html5/expected/egg.png \
  --screenshot egg.png \
  --output build/render-test/egg \
  --likeness ${DEFAULT_LIKENESS}
