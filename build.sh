#!/bin/bash
set -e
export JAVA_HOME=$(/usr/libexec/java_home)
mkdir -p build
cd build
cmake ..
cmake --build .