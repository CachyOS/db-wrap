# Set a default build type if none was specified
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  message(STATUS "Setting build type to 'RelWithDebInfo' as none was specified.")
  set(CMAKE_BUILD_TYPE
      RelWithDebInfo
      CACHE STRING "Choose the type of build." FORCE)
  # Set the possible values of build type for cmake-gui, ccmake
  set_property(
    CACHE CMAKE_BUILD_TYPE
    PROPERTY STRINGS
             "Debug"
             "Release"
             "MinSizeRel"
             "RelWithDebInfo")
endif()

set(DB_WRAP_MAIN_PROJECT OFF)
set(DB_WRAP_SUB_PROJECT ON)
if(CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
  set(DB_WRAP_MAIN_PROJECT ON)
  set(DB_WRAP_SUB_PROJECT OFF)
  message("Building DBWRAP as main project")

  set(CMAKE_CXX_STANDARD 20)
  set(CMAKE_CXX_STANDARD_REQUIRED ON)
  set(CMAKE_CXX_EXTENSIONS OFF)
endif()

option(DB_WRAP_ENABLE_TESTING "Enable tests" ${DB_WRAP_MAIN_PROJECT})
option(DB_WRAP_ENABLE_EXAMPLES "Enable examples" ${DB_WRAP_MAIN_PROJECT})
option(DB_WRAP_USE_EXTERNAL_LIBPQXX "Use an external libpqxx library" ${DB_WRAP_SUB_PROJECT})
option(DB_WRAP_USE_EXTERNAL_PFR "Use external pfr library" ${DB_WRAP_SUB_PROJECT})
option(DB_WRAP_USE_EXTERNAL_DOCTEST "Use external doctest library" ${DB_WRAP_SUB_PROJECT})

# enable color diagnostics
if(DB_WRAP_MAIN_PROJECT)
  if(CMAKE_CXX_COMPILER_ID MATCHES ".*Clang")
    add_compile_options(-fcolor-diagnostics)
  elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    add_compile_options(-fdiagnostics-color=always)
  else()
    message(STATUS "No colored compiler diagnostic set for '${CMAKE_CXX_COMPILER_ID}' compiler.")
  endif()
endif()

# Enables STL container checker if not building a release.
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
  add_definitions(-D_GLIBCXX_ASSERTIONS)
  add_definitions(-D_LIBCPP_ENABLE_THREAD_SAFETY_ANNOTATIONS=1)
  add_definitions(-D_LIBCPP_ENABLE_ASSERTIONS=1)
endif()

set(DB_WRAP_TARGET_NAME                ${PROJECT_NAME})
set(DB_WRAP_CONFIG_INSTALL_DIR         "${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}" CACHE INTERNAL "")
set(DB_WRAP_INCLUDE_BUILD_DIR          "${PROJECT_SOURCE_DIR}/include")
set(DB_WRAP_INCLUDE_INSTALL_DIR        "${CMAKE_INSTALL_INCLUDEDIR}")
set(DB_WRAP_TARGETS_EXPORT_NAME        "${PROJECT_NAME}-targets")
set(DB_WRAP_CMAKE_CONFIG_TEMPLATE      "${PROJECT_SOURCE_DIR}/cmake/db-wrap-config.cmake.in")
set(DB_WRAP_CMAKE_CONFIG_DIR           "${CMAKE_CURRENT_BINARY_DIR}")
set(DB_WRAP_CMAKE_VERSION_CONFIG_FILE  "${DB_WRAP_CMAKE_CONFIG_DIR}/${PROJECT_NAME}-config-version.cmake")
set(DB_WRAP_CMAKE_PROJECT_CONFIG_FILE  "${DB_WRAP_CMAKE_CONFIG_DIR}/${PROJECT_NAME}-config.cmake")
set(DB_WRAP_CMAKE_PROJECT_TARGETS_FILE "${DB_WRAP_CMAKE_CONFIG_DIR}/${PROJECT_NAME}-targets.cmake")
