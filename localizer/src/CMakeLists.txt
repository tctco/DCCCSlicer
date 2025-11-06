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

# Attempt to locate oneTBB libraries that are required by the runtime
find_package(onetbb CONFIG QUIET)

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

if(TARGET onetbb::onetbb)
    target_link_libraries(CentiloidCalculator PRIVATE onetbb::onetbb)
endif()

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

if(APPLE)
  set(_runtime_origin "@loader_path")
else()
  set(_runtime_origin "\$ORIGIN")
endif()

set_target_properties(CentiloidCalculator PROPERTIES
  BUILD_RPATH "${_runtime_origin}"
  INSTALL_RPATH "${_runtime_origin}"
  INSTALL_RPATH_USE_LINK_PATH TRUE
)

install(TARGETS CentiloidCalculator
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  COMPONENT Runtime
  RUNTIME_DEPENDENCY_SET CentiloidCalculatorRuntimeDeps
)

set(_runtime_dependency_args
  DESTINATION ${CMAKE_INSTALL_BINDIR}
  COMPONENT Runtime
)

set(_runtime_dependency_directories)
if(TARGET onnxruntime::onnxruntime)
  list(APPEND _runtime_dependency_directories
    $<TARGET_FILE_DIR:onnxruntime::onnxruntime>)
endif()

if(TARGET onetbb::onetbb)
  list(APPEND _runtime_dependency_directories
    $<TARGET_FILE_DIR:onetbb::onetbb>)
endif()

if(_runtime_dependency_directories)
  list(APPEND _runtime_dependency_args
    DIRECTORIES ${_runtime_dependency_directories})
endif()

install(RUNTIME_DEPENDENCY_SET CentiloidCalculatorRuntimeDeps
  ${_runtime_dependency_args}
)

install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/assets/"
  DESTINATION "${CMAKE_INSTALL_BINDIR}/assets"
  COMPONENT Runtime
)

# UCRT required for Windows


# Output information
message(STATUS "ITK_LIBRARIES: ${ITK_LIBRARIES}")
message(STATUS "CMAKE_INSTALL_PREFIX: ${CMAKE_INSTALL_PREFIX}")
message(STATUS "Project configured for refactored CentiloidCalculator v${PROJECT_VERSION}")

