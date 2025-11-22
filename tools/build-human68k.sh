#!/bin/sh
set -e
project_root=$(dirname "$0")/..
cmake -S ${project_root} -B ${project_root}/build-human68k -G Ninja -D CMAKE_BUILD_TYPE=Release -D CMAKE_TOOLCHAIN_FILE=./cmake/human68k.cmake
ninja -C ${project_root}/build-human68k
