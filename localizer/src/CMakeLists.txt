# CMakeList.txt: CentiloidCalculator Refactored Version
cmake_minimum_required(VERSION 3.21)

project(CentiloidCalculator VERSION 2.4.0)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Dependencies via conan
find_package(ITK CONFIG REQUIRED)
find_package(argparse CONFIG REQUIRED)
find_package(onnxruntime CONFIG REQUIRED)
find_package(tomlplusplus CONFIG REQUIRED)
find_package(Eigen3 CONFIG REQUIRED)

# Include directories
# include_directories(${CMAKE_CURRENT_SOURCE_DIR})

# Create executable
add_executable(CentiloidCalculator main.cpp)

# Add interface headers
target_sources(CentiloidCalculator PRIVATE
    interfaces/ISpatialNormalizer.h
    interfaces/IMetricCalculator.h
    interfaces/IConfiguration.h
)

# Add configuration management
target_sources(CentiloidCalculator PRIVATE
    config/Configuration.h
    config/Configuration.cpp
)

# Add metric calculators
target_sources(CentiloidCalculator PRIVATE
    calculators/SUVrCalculator.h
    calculators/SUVrCalculator.cpp
    calculators/CentiloidCalculator.h
    calculators/CentiloidCalculator.cpp
    calculators/CenTauRCalculator.h
    calculators/CenTauRCalculator.cpp
    calculators/CenTauRzCalculator.h
    calculators/CenTauRzCalculator.cpp
)

# Add spatial normalizers
target_sources(CentiloidCalculator PRIVATE
    normalizers/RigidVoxelMorphNormalizer.h
    normalizers/RigidVoxelMorphNormalizer.cpp
    normalizers/RegistrationPipeline.h
    normalizers/RegistrationPipeline.cpp
    normalizers/ImagePreprocessor.h
    normalizers/ImagePreprocessor.cpp
    normalizers/RigidRegistrationEngine.h
    normalizers/RigidRegistrationEngine.cpp
    normalizers/NonlinearRegistrationEngine.h
    normalizers/NonlinearRegistrationEngine.cpp
)

# Add factories
target_sources(CentiloidCalculator PRIVATE
    factories/SpatialNormalizerFactory.h
    factories/SpatialNormalizerFactory.cpp
    factories/MetricCalculatorFactory.h
    factories/MetricCalculatorFactory.cpp
)

# Add processing pipeline
target_sources(CentiloidCalculator PRIVATE
    pipeline/ProcessingPipeline.h
    pipeline/ProcessingPipeline.cpp
)

# Add utility sources
target_sources(CentiloidCalculator PRIVATE
    utils/common.h
    utils/common.cpp
)

# Add decoupler sources
target_sources(CentiloidCalculator PRIVATE
    decouplers/Decoupler.h
    decouplers/Decoupler.cpp
)

# Link libraries
target_link_libraries(CentiloidCalculator PRIVATE
    itk::itk
    onnxruntime::onnxruntime
    argparse::argparse
    tomlplusplus::tomlplusplus
    Eigen3::Eigen
)

# Compile options
if(MSVC)
    target_compile_options(CentiloidCalculator PRIVATE /W4)
else()
    target_compile_options(CentiloidCalculator PRIVATE -Wall -Wextra -Wpedantic)
endif()

# Set install prefix
set(CMAKE_INSTALL_PREFIX "E:/projects/paper/CentiloidCalculatorInstall" CACHE PATH "Installation directory")

include(InstallRequiredSystemLibraries)
include(GNUInstallDirs)

set(centiloid_runtime_dependency_args)
set(centiloid_install_target_args
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  COMPONENT Runtime
)

if(APPLE)
  set_target_properties(CentiloidCalculator PROPERTIES
    BUILD_RPATH "@loader_path"
    INSTALL_RPATH "@loader_path"
  )

  list(PREPEND centiloid_runtime_dependency_args
    RUNTIME_DEPENDENCY_SET
    calc_deps
  )
elseif(UNIX)
  # Use $ORIGIN so the executable can locate co-located shared libraries.
  set_target_properties(CentiloidCalculator PROPERTIES
    BUILD_RPATH "$ORIGIN"
    INSTALL_RPATH "$ORIGIN"
  )

  list(PREPEND centiloid_runtime_dependency_args
    RUNTIME_DEPENDENCY_SET
    calc_deps
  )
endif()

if(centiloid_runtime_dependency_args)
  list(PREPEND centiloid_install_target_args ${centiloid_runtime_dependency_args})
endif()

install(TARGETS CentiloidCalculator
  ${centiloid_install_target_args}
)

install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/assets/"
  DESTINATION "${CMAKE_INSTALL_BINDIR}/assets"
  COMPONENT Runtime
)

# Install dependent runtime DLLs next to the executable (requires CMake 3.21+)
if(WIN32)
  install(FILES $<TARGET_RUNTIME_DLLS:CentiloidCalculator>
    DESTINATION ${CMAKE_INSTALL_BINDIR}
    COMPONENT Runtime)
endif()

if(UNIX)
  install(RUNTIME_DEPENDENCY_SET calc_deps
    PRE_EXCLUDE_REGEXES
      "^libc\\.so" "^libm\\.so" "^libpthread\\.so" "^libdl\\.so"
      "^librt\\.so" "^libgcc_s\\.so" "^libstdc\\+\\+\\.so" "^ld-linux.*\\.so"
      "^libSystem\\.B\\.dylib" "^libobjc\\.A\\.dylib"
    POST_INCLUDE_REGEXES
      "libtbb.*" "libonnxruntime.*" "libITK.*"
    LIBRARY DESTINATION ${CMAKE_INSTALL_BINDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  )
endif()

# UCRT required for Windows


# Output information
message(STATUS "ITK_LIBRARIES: ${ITK_LIBRARIES}")
message(STATUS "CMAKE_INSTALL_PREFIX: ${CMAKE_INSTALL_PREFIX}")
message(STATUS "Project configured for refactored CentiloidCalculator v${PROJECT_VERSION}")

