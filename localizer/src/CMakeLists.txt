# CMakeList.txt: CentiloidCalculator Refactored Version
cmake_minimum_required(VERSION 3.21)

project(CentiloidCalculator VERSION 3.4.0)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# for VS Code Intellisense
# https://stackoverflow.com/questions/58077908/linking-conan-include-to-vs-code
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Dependencies via conan
find_package(ITK CONFIG REQUIRED)
find_package(argparse CONFIG REQUIRED)
find_package(onnxruntime CONFIG REQUIRED)
find_package(tomlplusplus CONFIG REQUIRED)
find_package(Eigen3 CONFIG REQUIRED)
find_package(rapidcsv CONFIG REQUIRED)

# Include directories
# include_directories(${CMAKE_CURRENT_SOURCE_DIR})

# Configure version header
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/core/config/Version.h.in
    ${CMAKE_CURRENT_SOURCE_DIR}/core/config/Version.h
    @ONLY
)

# Create executable
add_executable(CentiloidCalculator main.cpp)

# Add interface headers
target_sources(CentiloidCalculator PRIVATE
    core/interfaces/ISpatialNormalizer.h
    core/interfaces/IMetricCalculator.h
    core/interfaces/IConfiguration.h
)

# Add configuration management
target_sources(CentiloidCalculator PRIVATE
    core/config/Configuration.h
    core/config/Configuration.cpp
    core/config/ConfigLoader.h
    core/config/ConfigLoader.cpp
    core/config/Version.h
)

# Add spatial normalizers
target_sources(CentiloidCalculator PRIVATE
    normalizers/RigidVoxelMorphNormalizer.h
    normalizers/RigidVoxelMorphNormalizer.cpp
    normalizers/RegistrationPipeline.h
    normalizers/RegistrationPipeline.cpp
    normalizers/RigidRegistrationEngine.h
    normalizers/RigidRegistrationEngine.cpp
    normalizers/NonlinearRegistrationEngine.h
    normalizers/NonlinearRegistrationEngine.cpp
)

# Add image preprocessing module
target_sources(CentiloidCalculator PRIVATE
    preprocessing/ImagePreprocessor.h
    preprocessing/ImagePreprocessor.cpp
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
    pipeline/BatchProcessor.h
    pipeline/BatchProcessor.cpp
)

# Add utility sources
target_sources(CentiloidCalculator PRIVATE
    utils/common.h
    utils/common.cpp
    cli/Options.h
    cli/Options.cpp
    cli/CommandHelpers.h
    cli/CommandHelpers.cpp
    cli/Commands.h
    cli/Commands.cpp
    core/common/Common.h
    core/common/NiftiTypes.h
    core/common/NiftiIO.h
    core/common/NiftiIO.cpp
    core/common/ImageOps.h
    core/common/ImageOps.cpp
    core/common/PathUtils.h
    core/common/PathUtils.cpp
    core/common/Filesystem.h
    core/common/Filesystem.cpp
    core/common/Logging.h
    core/common/Logging.cpp
    core/common/OnnxPath.h
    core/common/ProcessingContracts.h
    core/interfaces/IMetricLogic.h
    core/interfaces/IMetricModuleRegistry.h
    core/interfaces/IMetricCLI.h
    core/interfaces/ISpatialNormalizationCLI.h
    core/di/ServiceContainer.h
    core/providers/LegacyNormalizerProvider.h
    core/providers/LegacyNormalizerProvider.cpp
    core/services/ISpatialNormalizationService.h
    core/services/SpatialNormalizationService.h
    core/services/SpatialNormalizationService.cpp
    core/services/IMetricService.h
    core/services/MetricService.h
    core/services/MetricService.cpp
    core/services/MetricModuleRegistry.h
    core/services/MetricModuleRegistry.cpp
    core/services/IFileService.h
    core/services/FileService.h
    core/services/FileService.cpp
    core/application/PipelineApplication.h
    core/application/PipelineApplication.cpp
    core/common/BatchLogging.h
    core/common/BatchLogging.cpp
    core/di/Bootstrap.h
    core/di/Bootstrap.cpp
    metrics/suvr/SUVrLogic.h
    metrics/suvr/SUVrLogic.cpp
    metrics/suvr/SUVrCLI.h
    metrics/suvr/SUVrCLI.cpp
    metrics/suvr/SUVrCalculator.h
    metrics/suvr/SUVrCalculator.cpp
    metrics/centiloid/CentiloidLogic.h
    metrics/centiloid/CentiloidLogic.cpp
    metrics/centiloid/CentiloidCLI.h
    metrics/centiloid/CentiloidCLI.cpp
    metrics/centiloid/CentiloidCalculator.h
    metrics/centiloid/CentiloidCalculator.cpp
    metrics/centaur/CenTauRLogic.h
    metrics/centaur/CenTauRLogic.cpp
    metrics/centaur/CenTauRCLI.h
    metrics/centaur/CenTauRCLI.cpp
    metrics/centaur/CenTauRCalculator.h
    metrics/centaur/CenTauRCalculator.cpp
    metrics/centaurz/CentaurzLogic.h
    metrics/centaurz/CentaurzLogic.cpp
    metrics/centaurz/CentaurzCLI.h
    metrics/centaurz/CentaurzCLI.cpp
    metrics/centaurz/CenTauRzCalculator.h
    metrics/centaurz/CenTauRzCalculator.cpp
    metrics/fillstates/FillStatesLogic.h
    metrics/fillstates/FillStatesLogic.cpp
    metrics/fillstates/FillStatesCLI.h
    metrics/fillstates/FillStatesCLI.cpp
    metrics/fillstates/FillStatesCalculator.h
    metrics/fillstates/FillStatesCalculator.cpp
    metrics/adad/ADADLogic.h
    metrics/adad/ADADLogic.cpp
    metrics/adad/ADADCLI.h
    metrics/adad/ADADCLI.cpp
    metrics/adad/Decoupler.h
    metrics/adad/Decoupler.cpp
    metrics/ModuleCatalog.h
    metrics/ModuleCatalog.cpp
    spatialNormalizations/ModuleCatalog.h
    spatialNormalizations/ModuleCatalog.cpp
    spatialNormalizations/NullMetric.h
    spatialNormalizations/NullMetric.cpp
    spatialNormalizations/standard/NormalizeCLI.h
    spatialNormalizations/standard/NormalizeCLI.cpp
    spatialNormalizations/adni/AdniPetCoreCLI.h
    spatialNormalizations/adni/AdniPetCoreCLI.cpp
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

# Output information
message(STATUS "ITK_LIBRARIES: ${ITK_LIBRARIES}")
message(STATUS "CMAKE_INSTALL_PREFIX: ${CMAKE_INSTALL_PREFIX}")
message(STATUS "Project configured for refactored CentiloidCalculator v${PROJECT_VERSION}")