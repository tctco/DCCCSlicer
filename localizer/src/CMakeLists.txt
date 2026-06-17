# CMakeList.txt: DCCCcore Refactored Version
cmake_minimum_required(VERSION 3.21)

project(DCCCcore VERSION 4.2.3)
set(DCCCCORE_VERSION_SUFFIX "-alpha")
set(DCCCCORE_SOFTWARE_VERSION "${PROJECT_VERSION}${DCCCCORE_VERSION_SUFFIX}")

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
add_executable(DCCCcore main.cpp)

# Add interface headers
target_sources(DCCCcore PRIVATE
    core/interfaces/ISpatialNormalizer.h
    core/interfaces/IConfiguration.h
)

# Add configuration management
target_sources(DCCCcore PRIVATE
    core/config/Configuration.h
    core/config/Configuration.cpp
    core/config/ConfigLoader.h
    core/config/ConfigLoader.cpp
    core/config/Version.h
)

# Add spatial normalizers
target_sources(DCCCcore PRIVATE
    core/normalizers/RigidVoxelMorphNormalizer.h
    core/normalizers/RigidVoxelMorphNormalizer.cpp
    core/normalizers/RegistrationPipeline.h
    core/normalizers/RegistrationPipeline.cpp
    core/normalizers/RigidRegistrationEngine.h
    core/normalizers/RigidRegistrationEngine.cpp
    core/normalizers/NonlinearRegistrationEngine.h
    core/normalizers/NonlinearRegistrationEngine.cpp
)

# Add image preprocessing module
target_sources(DCCCcore PRIVATE
    core/preprocessing/ImagePreprocessor.h
    core/preprocessing/ImagePreprocessor.cpp
)

# Add utility sources
target_sources(DCCCcore PRIVATE
    core/common/Common.h
    core/common/ImageTypes.h
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
    core/common/NormalizationContracts.h
    core/common/OnnxPath.h
    core/interfaces/ISpatialNormalizationCLI.h
    core/di/ServiceContainer.h
    core/services/ISpatialNormalizationService.h
    core/services/SpatialNormalizationService.h
    core/services/SpatialNormalizationService.cpp
    core/services/IFileService.h
    core/services/FileService.h
    core/services/FileService.cpp
    core/di/Bootstrap.h
    core/di/Bootstrap.cpp
    metrics/shared/MetricTypes.h
    metrics/shared/MetricRunResult.h
    metrics/shared/IMetricCLI.h
    metrics/shared/MetricRegistry.h
    metrics/shared/MetricRegistry.cpp
    metrics/shared/BatchLogging.h
    metrics/shared/BatchLogging.cpp
    metrics/shared/SingleRunner.h
    metrics/shared/BatchRunner.h
    metrics/shared/DebugPathHelpers.h
    metrics/suvr/SUVrCLI.h
    metrics/suvr/SUVrCLI.cpp
    metrics/suvr/SUVrService.h
    metrics/suvr/SUVrService.cpp
    metrics/suvr/SUVrCalculator.h
    metrics/suvr/SUVrCalculator.cpp
    metrics/suvr/SUVrModule.h
    metrics/suvr/SUVrModule.cpp
    metrics/centaur/CenTauRCLI.h
    metrics/centaur/CenTauRCLI.cpp
    metrics/centaur/CenTauRService.h
    metrics/centaur/CenTauRService.cpp
    metrics/centaur/CenTauRCalculator.h
    metrics/centaur/CenTauRCalculator.cpp
    metrics/centaur/CenTauRModule.h
    metrics/centaur/CenTauRModule.cpp
    metrics/centaurz/CentaurzCLI.h
    metrics/centaurz/CentaurzCLI.cpp
    metrics/centaurz/CentaurzService.h
    metrics/centaurz/CentaurzService.cpp
    metrics/centaurz/CenTauRzCalculator.h
    metrics/centaurz/CenTauRzCalculator.cpp
    metrics/centaurz/CentaurzModule.h
    metrics/centaurz/CentaurzModule.cpp
    metrics/centiloid/CentiloidCLI.h
    metrics/centiloid/CentiloidCLI.cpp
    metrics/centiloid/CentiloidService.h
    metrics/centiloid/CentiloidService.cpp
    metrics/centiloid/CentiloidCalculator.h
    metrics/centiloid/CentiloidCalculator.cpp
    metrics/centiloid/CentiloidModule.h
    metrics/centiloid/CentiloidModule.cpp
    metrics/fillstates/FillStatesCLI.h
    metrics/fillstates/FillStatesCLI.cpp
    metrics/fillstates/FillStatesService.h
    metrics/fillstates/FillStatesService.cpp
    metrics/fillstates/FillStatesCalculator.h
    metrics/fillstates/FillStatesCalculator.cpp
    metrics/fillstates/FillStatesModule.h
    metrics/fillstates/FillStatesModule.cpp
    metrics/abetaindex/AbetaIndexCLI.h
    metrics/abetaindex/AbetaIndexCLI.cpp
    metrics/abetaindex/AbetaIndexService.h
    metrics/abetaindex/AbetaIndexService.cpp
    metrics/abetaindex/AbetaIndexCalculator.h
    metrics/abetaindex/AbetaIndexCalculator.cpp
    metrics/abetaindex/AbetaIndexModule.h
    metrics/abetaindex/AbetaIndexModule.cpp
    metrics/abetaload/AbetaLoadCLI.h
    metrics/abetaload/AbetaLoadCLI.cpp
    metrics/abetaload/AbetaLoadService.h
    metrics/abetaload/AbetaLoadService.cpp
    metrics/abetaload/AbetaLoadCalculator.h
    metrics/abetaload/AbetaLoadCalculator.cpp
    metrics/abetaload/AbetaLoadModule.h
    metrics/abetaload/AbetaLoadModule.cpp
    metrics/adad/ADADCLI.h
    metrics/adad/ADADCLI.cpp
    metrics/adad/ADADService.h
    metrics/adad/ADADService.cpp
    metrics/adad/ADADModule.h
    metrics/adad/ADADModule.cpp
    metrics/adad/Decoupler.h
    metrics/adad/Decoupler.cpp
    metrics/list/MetricsCLI.h
    metrics/list/MetricsCLI.cpp
    metrics/ModuleCatalog.h
    metrics/ModuleCatalog.cpp
    spatialNormalizations/ModuleCatalog.h
    spatialNormalizations/ModuleCatalog.cpp
    spatialNormalizations/CLIOptions.h
    spatialNormalizations/CLIOptions.cpp
    spatialNormalizations/standard/NormalizeCLI.h
    spatialNormalizations/standard/NormalizeCLI.cpp
    spatialNormalizations/adni/AdniPetCoreCLI.h
    spatialNormalizations/adni/AdniPetCoreCLI.cpp
    spatialNormalizations/rigid/RigidCLI.h
    spatialNormalizations/rigid/RigidCLI.cpp
)

# Link libraries
target_link_libraries(DCCCcore PRIVATE
    itk::itk
    onnxruntime::onnxruntime
    argparse::argparse
    tomlplusplus::tomlplusplus
    Eigen3::Eigen
    rapidcsv::rapidcsv
)

# Compile options
if(MSVC)
    target_compile_options(DCCCcore PRIVATE /W4)
else()
    target_compile_options(DCCCcore PRIVATE -Wall -Wextra -Wpedantic)
endif()

# Set install prefix
set(CMAKE_INSTALL_PREFIX "E:/projects/paper/DCCCcoreInstall" CACHE PATH "Installation directory")

include(InstallRequiredSystemLibraries)
include(GNUInstallDirs)

set(centiloid_runtime_dependency_args)
set(centiloid_install_target_args
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  COMPONENT Runtime
)

if(APPLE)
  set_target_properties(DCCCcore PROPERTIES
    BUILD_RPATH "@loader_path"
    INSTALL_RPATH "@loader_path"
  )

  list(PREPEND centiloid_runtime_dependency_args
    RUNTIME_DEPENDENCY_SET
    calc_deps
  )
elseif(UNIX)
  # Use $ORIGIN so the executable can locate co-located shared libraries.
  set_target_properties(DCCCcore PROPERTIES
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

install(TARGETS DCCCcore
  ${centiloid_install_target_args}
)

install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/assets/"
  DESTINATION "${CMAKE_INSTALL_BINDIR}/assets"
  COMPONENT Runtime
)

# Install dependent runtime DLLs next to the executable (requires CMake 3.21+)
if(WIN32)
  install(FILES $<TARGET_RUNTIME_DLLS:DCCCcore>
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
message(STATUS "Project configured for refactored DCCCcore v${DCCCCORE_SOFTWARE_VERSION}")
