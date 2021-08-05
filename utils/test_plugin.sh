#!/usr/bin/env bash

set -e

CLASS=com.dynamo.bob.pipeline.Rive
JAR=./defold-rive/plugins/share/pluginRiveExt.jar
BOB=~/work/defold/tmp/dynamo_home/share/java/bob.jar

java -cp $JAR:$BOB:./defold-rive/plugins/lib/x86_64-osx $CLASS $*
