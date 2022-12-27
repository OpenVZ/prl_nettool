#!/usr/bin/env bash

set -e

BUILD=build
COVERAGE=coverage

rm -rf $BUILD
mkdir $BUILD

pushd $BUILD
cmake .. -DCMAKE_BUILD_TYPE=Debug
make
popd

rm -rf $COVERAGE
mkdir $COVERAGE
$BUILD/rcconf_test
lcov --capture --directory . --output-file $COVERAGE/coverage.info
genhtml $COVERAGE/coverage.info --output-directory $COVERAGE/html
