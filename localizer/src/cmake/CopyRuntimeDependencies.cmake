cmake_minimum_required(VERSION 3.16)

if(NOT DEFINED TARGET_FILE OR TARGET_FILE STREQUAL "")
  message(FATAL_ERROR "TARGET_FILE parameter must be provided")
endif()

if(NOT EXISTS "${TARGET_FILE}")
  message(FATAL_ERROR "Target file '${TARGET_FILE}' does not exist")
endif()

if(NOT DEFINED OUTPUT_DIR OR OUTPUT_DIR STREQUAL "")
  message(FATAL_ERROR "OUTPUT_DIR parameter must be provided")
endif()

file(MAKE_DIRECTORY "${OUTPUT_DIR}")

set(_search_directories "")
if(DEFINED ADDITIONAL_SEARCH_DIRS AND NOT ADDITIONAL_SEARCH_DIRS STREQUAL "")
  set(_search_directories ${ADDITIONAL_SEARCH_DIRS})
endif()

set(_dependency_args
  RESOLVED_DEPENDENCIES_VAR _resolved_dependencies
  UNRESOLVED_DEPENDENCIES_VAR _unresolved_dependencies
  EXECUTABLES "${TARGET_FILE}"
)

if(_search_directories)
  list(APPEND _dependency_args DIRECTORIES ${_search_directories})
endif()

file(GET_RUNTIME_DEPENDENCIES ${_dependency_args})

if(_unresolved_dependencies)
  list(REMOVE_DUPLICATES _unresolved_dependencies)
  message(WARNING "Unresolved runtime dependencies for '${TARGET_FILE}': ${_unresolved_dependencies}")
endif()

if(NOT _resolved_dependencies)
  message(STATUS "No additional runtime dependencies discovered for '${TARGET_FILE}'")
  return()
endif()

list(REMOVE_DUPLICATES _resolved_dependencies)

foreach(_dependency IN LISTS _resolved_dependencies)
  if(NOT EXISTS "${_dependency}")
    continue()
  endif()

  get_filename_component(_dependency_name "${_dependency}" NAME)
  set(_destination_path "${OUTPUT_DIR}/${_dependency_name}")

  if(EXISTS "${_destination_path}")
    continue()
  endif()

  message(STATUS "Copying runtime dependency '${_dependency_name}'")
  file(INSTALL
    DESTINATION "${OUTPUT_DIR}"
    TYPE SHARED_LIBRARY
    FILES "${_dependency}"
  )
endforeach()
