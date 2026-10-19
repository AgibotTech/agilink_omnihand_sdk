# Locate an installed or packaged OmniHand C++ SDK.
#
# Usage:
#   list(APPEND CMAKE_MODULE_PATH "/path/to/share/cmake/omnihand")
#   find_package(omnihand REQUIRED)
#   target_link_libraries(my_target PRIVATE omnihand)

get_filename_component(_omnihand_cmake_dir "${CMAKE_CURRENT_LIST_FILE}" DIRECTORY)
get_filename_component(OMNIHAND_ROOT "${_omnihand_cmake_dir}/../../.." ABSOLUTE)

set(omnihand_INCLUDE_DIRS "${OMNIHAND_ROOT}/include")
set(_omnihand_lib_dir "${OMNIHAND_ROOT}/lib")
set(_omnihand_bin_dir "${OMNIHAND_ROOT}/bin")

find_library(OMNIHAND_LIBRARY
  NAMES omnihand
  HINTS "${_omnihand_lib_dir}"
  NO_DEFAULT_PATH)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(omnihand
  REQUIRED_VARS omnihand_INCLUDE_DIRS OMNIHAND_LIBRARY)

if(omnihand_FOUND)
  if(NOT TARGET omnihand)
    add_library(omnihand SHARED IMPORTED)
    if(WIN32)
      set_target_properties(omnihand PROPERTIES
        IMPORTED_IMPLIB "${OMNIHAND_LIBRARY}"
        IMPORTED_LOCATION "${_omnihand_bin_dir}/omnihand.dll"
        INTERFACE_INCLUDE_DIRECTORIES "${omnihand_INCLUDE_DIRS}")
    else()
      set_target_properties(omnihand PROPERTIES
        IMPORTED_LOCATION "${OMNIHAND_LIBRARY}"
        IMPORTED_NO_SONAME TRUE
        INTERFACE_INCLUDE_DIRECTORIES "${omnihand_INCLUDE_DIRS}")
    endif()
  endif()

  find_library(AGILINK_LOGGER_LIBRARY
    NAMES agilink_logger
    HINTS "${_omnihand_lib_dir}"
    NO_DEFAULT_PATH)
  if(AGILINK_LOGGER_LIBRARY AND NOT TARGET agilink_logger)
    add_library(agilink_logger SHARED IMPORTED)
    if(WIN32)
      set_target_properties(agilink_logger PROPERTIES
        IMPORTED_IMPLIB "${AGILINK_LOGGER_LIBRARY}"
        IMPORTED_LOCATION "${_omnihand_bin_dir}/agilink_logger.dll"
        INTERFACE_INCLUDE_DIRECTORIES "${omnihand_INCLUDE_DIRS}")
    else()
      set_target_properties(agilink_logger PROPERTIES
        IMPORTED_LOCATION "${AGILINK_LOGGER_LIBRARY}"
        IMPORTED_NO_SONAME TRUE
        INTERFACE_INCLUDE_DIRECTORIES "${omnihand_INCLUDE_DIRS}")
    endif()
  endif()
  if(TARGET agilink_logger)
    set_property(TARGET omnihand APPEND PROPERTY
      INTERFACE_LINK_LIBRARIES agilink_logger)
  endif()
endif()

unset(_omnihand_cmake_dir)
unset(_omnihand_lib_dir)
unset(_omnihand_bin_dir)
