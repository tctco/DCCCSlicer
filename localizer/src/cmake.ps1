# Run with Developer PowerShell for VS2
# conan profile detect --force

# remove build and install directories
Remove-Item -Recurse -Force build
Remove-Item -Recurse -Force install

conan install . --output-folder=build --build=missing -s build_type=Release `
  -s compiler.cppstd=17 `
  -c tools.cmake.cmaketoolchain:generator=Ninja

cmake -S . `
  -B build `
  -G "Ninja" `
  -DCMAKE_TOOLCHAIN_FILE="./build/conan_toolchain.cmake" `
  -DCMAKE_BUILD_TYPE=Release `
  -DCMAKE_INSTALL_PREFIX="./install"

cmake --build build --config Release
cmake --install build --config Release