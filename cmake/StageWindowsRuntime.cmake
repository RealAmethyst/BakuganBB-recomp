# SPDX-License-Identifier: BSD-3-Clause
# Inspect runtime imports only; never load or execute the application or Prism.
cmake_minimum_required(VERSION 3.25)
if(POLICY CMP0207)
  cmake_policy(SET CMP0207 NEW)
endif()

if(NOT CMAKE_HOST_WIN32)
  message(FATAL_ERROR "Windows runtime dependency inspection requires Windows")
endif()
foreach(required SEARCH_DIRECTORY LIBRARIES OUTPUT UNRESOLVED_OUTPUT)
  if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
    message(FATAL_ERROR "Missing runtime inspection argument: ${required}")
  endif()
endforeach()
find_program(runtime_dumpbin NAMES dumpbin REQUIRED)
set(CMAKE_GET_RUNTIME_DEPENDENCIES_PLATFORM "windows+pe")
set(CMAKE_GET_RUNTIME_DEPENDENCIES_TOOL "dumpbin")
set(CMAKE_GET_RUNTIME_DEPENDENCIES_COMMAND "${runtime_dumpbin}")

file(REAL_PATH "$ENV{SystemRoot}/System32" system_directory)
if(NOT EXISTS "${system_directory}/kernel32.dll")
  message(FATAL_ERROR "The Windows system DLL directory could not be verified")
endif()
# Exclude actual resolved system files, never a DLL-name allowlist. This also
# avoids recursing into Windows' internal API-set implementation dependencies.
file(GLOB system_libraries "${system_directory}/*.dll")
set(executable_arguments)
if(DEFINED EXECUTABLE AND NOT EXECUTABLE STREQUAL "")
  list(APPEND executable_arguments EXECUTABLES "${EXECUTABLE}")
endif()
file(GET_RUNTIME_DEPENDENCIES
  ${executable_arguments}
  LIBRARIES ${LIBRARIES}
  DIRECTORIES "${SEARCH_DIRECTORY}"
  RESOLVED_DEPENDENCIES_VAR resolved
  UNRESOLVED_DEPENDENCIES_VAR unresolved
  CONFLICTING_DEPENDENCIES_PREFIX conflicts
  POST_EXCLUDE_FILES ${system_libraries})
if(conflicts_FILENAMES)
  message(FATAL_ERROR "Conflicting Windows runtime imports: ${conflicts_FILENAMES}")
endif()
file(REAL_PATH "${SEARCH_DIRECTORY}" allowed_directory)
foreach(dependency IN LISTS resolved)
  file(REAL_PATH "${dependency}" real_dependency)
  get_filename_component(dependency_directory "${real_dependency}" DIRECTORY)
  string(TOLOWER "${dependency_directory}" dependency_directory)
  string(TOLOWER "${allowed_directory}" expected_directory)
  if(NOT dependency_directory STREQUAL expected_directory)
    message(FATAL_ERROR "Dependency resolved outside the verified build output: ${dependency}")
  endif()
endforeach()
list(JOIN resolved "\n" report)
file(WRITE "${OUTPUT}" "${report}")
# The Python caller must validate every unresolved name. API-set contracts may
# have no disk file; only the documented OS availability query can accept them.
list(JOIN unresolved "\n" unresolved_report)
file(WRITE "${UNRESOLVED_OUTPUT}" "${unresolved_report}")
