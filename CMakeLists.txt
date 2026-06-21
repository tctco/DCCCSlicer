cmake_minimum_required(VERSION 3.16.3...3.19.7 FATAL_ERROR)

project(DCCCSlicer)

#-----------------------------------------------------------------------------
# Extension meta-information
set(EXTENSION_HOMEPAGE "https://github.com/tctco/DCCCSlicer")
set(EXTENSION_CATEGORY "Examples")
set(EXTENSION_CONTRIBUTORS "Cheng Tang")
set(EXTENSION_DESCRIPTION "An open-source, super-simple, ultra-fast, fully-automated, fairly-accurate and PET-only solution to conduct spatial normalization and semi-quantification for almost any brain PET modalities.")
set(EXTENSION_ICONURL "https://www.example.com/Slicer/Extensions/ACPCLocalizer.png")
set(EXTENSION_SCREENSHOTURLS "https://www.example.com/Slicer/Extensions/ACPCLocalizer/Screenshots/1.png")
set(EXTENSION_DEPENDS "NA") # Specified as a list or "NA" if no dependencies

#-----------------------------------------------------------------------------
# Extension dependencies
find_package(Slicer REQUIRED)
include(${Slicer_USE_FILE})

#-----------------------------------------------------------------------------
# Extension modules
add_subdirectory(localizer)
## NEXT_MODULE

#-----------------------------------------------------------------------------
include(${Slicer_EXTENSION_GENERATE_CONFIG})
include(${Slicer_EXTENSION_CPACK})
