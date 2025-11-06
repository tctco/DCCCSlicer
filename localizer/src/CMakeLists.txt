# CMakeList.txt: CentiloidCalculator Refactored Version
cmake_minimum_required(VERSION 3.16)

project(CentiloidCalculator VERSION 2.4.0)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(UNIX)
    set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)
endif()

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

set(CENTILOID_RUNTIME_DEPS_DIR "${CMAKE_CURRENT_BINARY_DIR}/runtime_deps")
file(MAKE_DIRECTORY "${CENTILOID_RUNTIME_DEPS_DIR}")

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

add_custom_command(TARGET CentiloidCalculator POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E echo "Collecting CentiloidCalculator runtime dependencies"
    COMMAND ${CMAKE_COMMAND}
        -DTARGET_FILE=$<TARGET_FILE:CentiloidCalculator>
        -DOUTPUT_DIR=${CENTILOID_RUNTIME_DEPS_DIR}
        -DADDITIONAL_SEARCH_DIRS=$<TARGET_FILE_DIR:CentiloidCalculator>
        -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/CopyRuntimeDependencies.cmake"
    BYPRODUCTS "${CENTILOID_RUNTIME_DEPS_DIR}"
    VERBATIM
)

# Ensure the executable can discover bundled shared libraries at runtime
if(APPLE)
    set_target_properties(CentiloidCalculator PROPERTIES
        BUILD_RPATH "@loader_path"
        INSTALL_RPATH "@loader_path"
    )
elseif(UNIX)
    set_target_properties(CentiloidCalculator PROPERTIES
        BUILD_RPATH "\$ORIGIN"
        INSTALL_RPATH "\$ORIGIN"
    )
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

install(TARGETS CentiloidCalculator
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  COMPONENT Runtime
)

install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/assets/"
  DESTINATION "${CMAKE_INSTALL_BINDIR}/assets"
  COMPONENT Runtime
)

install(DIRECTORY "${CENTILOID_RUNTIME_DEPS_DIR}/"
  DESTINATION ${CMAKE_INSTALL_BINDIR}
  COMPONENT Runtime
  FILES_MATCHING
    PATTERN "*.dll"
    PATTERN "*.dylib"
    PATTERN "*.so*"
  OPTIONAL
)

# UCRT required for Windows


# Output information
message(STATUS "ITK_LIBRARIES: ${ITK_LIBRARIES}")
message(STATUS "CMAKE_INSTALL_PREFIX: ${CMAKE_INSTALL_PREFIX}")
message(STATUS "Project configured for refactored CentiloidCalculator v${PROJECT_VERSION}")

