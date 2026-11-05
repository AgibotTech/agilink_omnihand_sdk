# Locate an installed or packaged OmniHand C++ SDK.
#
# Usage:
#   list(APPEND CMAKE_MODULE_PATH "/path/to/share/cmake/omnihand")
#   find_package(omnihand REQUIRED)
#   target_link_libraries(my_target PRIVATE omnihand)
#   target_link_libraries(my_client PRIVATE omnihand_client)     # optional
#   target_link_libraries(my_server_host PRIVATE omnihand_server) # optional

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

  # Shared REST/WebSocket value and JSON codec used by both public targets.
  find_library(OMNIHAND_WEB_LIBRARY
    NAMES omnihand_web
    HINTS "${_omnihand_lib_dir}"
    NO_DEFAULT_PATH)
  if(OMNIHAND_WEB_LIBRARY AND NOT TARGET omnihand_web)
    add_library(omnihand_web STATIC IMPORTED)
    set_target_properties(omnihand_web PROPERTIES
      IMPORTED_LOCATION "${OMNIHAND_WEB_LIBRARY}"
      INTERFACE_INCLUDE_DIRECTORIES "${omnihand_INCLUDE_DIRS}")
  endif()

  # The embeddable server is optional for package discovery, but is installed
  # by the full SDK. Its public header contains no Crow/JSON implementation
  # types; consumers only link this single archive and the core SDK target.
  find_library(OMNIHAND_SERVER_LIBRARY
    NAMES omnihand_server
    HINTS "${_omnihand_lib_dir}"
    NO_DEFAULT_PATH)
  if(OMNIHAND_SERVER_LIBRARY AND NOT TARGET omnihand_server)
    add_library(omnihand_server STATIC IMPORTED)
    set(_omnihand_server_links omnihand)
    if(TARGET omnihand_web)
      list(APPEND _omnihand_server_links omnihand_web)
    endif()
    if(UNIX)
      find_package(Threads REQUIRED)
      list(APPEND _omnihand_server_links Threads::Threads)
    endif()
    set_target_properties(omnihand_server PROPERTIES
      IMPORTED_LOCATION "${OMNIHAND_SERVER_LIBRARY}"
      INTERFACE_INCLUDE_DIRECTORIES "${omnihand_INCLUDE_DIRS}"
      INTERFACE_LINK_LIBRARIES "${_omnihand_server_links}")
    unset(_omnihand_server_links)
  endif()

  # Lightweight remote client. It intentionally does not link omnihand or any
  # hardware transport library.
  find_library(OMNIHAND_CLIENT_LIBRARY
    NAMES omnihand_client
    HINTS "${_omnihand_lib_dir}"
    NO_DEFAULT_PATH)
  if(OMNIHAND_CLIENT_LIBRARY AND NOT TARGET omnihand_client)
    add_library(omnihand_client STATIC IMPORTED)
    set(_omnihand_client_links "")
    if(TARGET omnihand_web)
      list(APPEND _omnihand_client_links omnihand_web)
    endif()
    if(WIN32)
      list(APPEND _omnihand_client_links ws2_32)
    elseif(UNIX)
      find_package(Threads REQUIRED)
      list(APPEND _omnihand_client_links Threads::Threads)
    endif()
    set_target_properties(omnihand_client PROPERTIES
      IMPORTED_LOCATION "${OMNIHAND_CLIENT_LIBRARY}"
      INTERFACE_INCLUDE_DIRECTORIES "${omnihand_INCLUDE_DIRS}"
      INTERFACE_LINK_LIBRARIES "${_omnihand_client_links}")
    unset(_omnihand_client_links)
  endif()
endif()

unset(_omnihand_cmake_dir)
unset(_omnihand_lib_dir)
unset(_omnihand_bin_dir)
