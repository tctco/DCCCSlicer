#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "${BASH_SOURCE[0]}")/../localizer/src"

BUILD_TYPE="${BUILD_TYPE:-Release}"
INSTALL_PREFIX="${INSTALL_PREFIX:-./install}"
CONAN_CPPSTD="${CONAN_CPPSTD:-gnu17}"
CONAN_BUILD_ARGS="${CONAN_BUILD_ARGS:---build=missing}"
# Split the simple, space-delimited Conan build policy string into argv items.
# This lets the legacy Linux path force known build tools such as b2 to be
# rebuilt inside the manylinux2014 container instead of downloading binaries
# that may have been produced on newer glibc systems.
read -r -a conan_build_args <<< "${CONAN_BUILD_ARGS}"

sudo mkdir -p build "${CONAN_HOME:-/home/dev/.conan2}"
sudo chown -R "$(id -u):$(id -g)" build "${CONAN_HOME:-/home/dev/.conan2}"

conan profile detect --force
conan install . \
  --output-folder=build \
  "${conan_build_args[@]}" \
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
