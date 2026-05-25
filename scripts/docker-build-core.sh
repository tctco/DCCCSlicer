#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "${BASH_SOURCE[0]}")/../localizer/src"

BUILD_TYPE="${BUILD_TYPE:-Release}"
INSTALL_PREFIX="${INSTALL_PREFIX:-./install}"
CONAN_CPPSTD="${CONAN_CPPSTD:-gnu17}"

sudo mkdir -p build "${CONAN_HOME:-/home/dev/.conan2}"
sudo chown -R "$(id -u):$(id -g)" build "${CONAN_HOME:-/home/dev/.conan2}"

conan profile detect --force
conan install . \
  --output-folder=build \
  --build=missing \
  -s build_type="${BUILD_TYPE}" \
  -s compiler.cppstd="${CONAN_CPPSTD}" \
  -c tools.cmake.cmaketoolchain:generator=Ninja \
  -o onetbb/*:tbbmalloc=False \
  -o onetbb/*:tbbproxy=False

cmake -S . \
  -B build \
  -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE=build/conan_toolchain.cmake \
  -DCMAKE_BUILD_TYPE="${BUILD_TYPE}" \
  -DCMAKE_INSTALL_PREFIX="${INSTALL_PREFIX}"

cmake --build build --config "${BUILD_TYPE}"
cmake --install build --config "${BUILD_TYPE}"
