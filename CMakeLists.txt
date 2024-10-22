cmake_minimum_required(VERSION 3.16)

project(db-wrap
        VERSION 1.0.0
        LANGUAGES CXX)

# includes
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)
include("${PROJECT_SOURCE_DIR}/cmake/ProjectOptions.cmake")
include("${PROJECT_SOURCE_DIR}/cmake/CompilerWarnings.cmake")
include("${PROJECT_SOURCE_DIR}/cmake/CPM.cmake")
include("${PROJECT_SOURCE_DIR}/cmake/FetchDepends.cmake")

# options
add_library(project_options INTERFACE)
target_compile_features(project_options INTERFACE cxx_std_20)

# library targets
add_library(db-wrap INTERFACE)
add_library(db-wrap::db-wrap ALIAS db-wrap)

target_include_directories(
    db-wrap
    INTERFACE
    $<BUILD_INTERFACE:${DB_WRAP_INCLUDE_BUILD_DIR}>
    $<INSTALL_INTERFACE:include>
)

# enable warnings
add_library(project_warnings INTERFACE)
set_project_warnings(project_warnings)

target_link_libraries(db-wrap INTERFACE project_warnings project_options libpqxx::pqxx Boost::pfr)

# unit tests
if (DB_WRAP_ENABLE_TESTING)
  message(STATUS "DBWRAP: unit-tests enabled")
  add_subdirectory(tests)
endif()

# examples
if (DB_WRAP_ENABLE_EXAMPLES)
  message(STATUS "DBWRAP: examples enabled")
  add_subdirectory(examples)
endif()

# docs gen
#find_package(Doxygen QUIET)
#if (DOXYGEN_FOUND)
#    set(DOXYGEN_GENERATE_HTML YES)
#    set(DOXYGEN_HTML_OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/docs")
#    set(DOXYGEN_BUILTIN_STL_SUPPORT YES)
#    set(DOXYGEN_USE_MDFILE_AS_MAINPAGE README.md)
#
#    # ref https://github.com/PacktPublishing/Modern-CMake-for-Cpp-2E/blob/main/examples/ch13/02-doxygen-nice/cmake/Doxygen.cmake
#    FetchContent_Declare(doxygen-awesome-css
#        GIT_REPOSITORY
#          https://github.com/jothepro/doxygen-awesome-css.git
#        GIT_TAG
#          v2.3.1
#    )
#    FetchContent_MakeAvailable(doxygen-awesome-css)
#    FetchContent_GetProperties(doxygen-awesome-css SOURCE_DIR AWESOME_CSS_DIR)
#
#    # Customize Doxygen settings for enhanced output
#    set(DOXYGEN_GENERATE_TREEVIEW YES)
#    set(DOXYGEN_DISABLE_INDEX NO)
#    set(DOXYGEN_FULL_SIDEBAR NO)
#    set(DOXYGEN_HTML_COLORSTYLE LIGHT)
#
#    # Use Doxygen Awesome CSS for styling
#    set(DOXYGEN_HTML_EXTRA_STYLESHEET "${AWESOME_CSS_DIR}/doxygen-awesome.css")
#    set(DOXYGEN_HTML_EXTRA_FILES
#        "${AWESOME_CSS_DIR}/doxygen-awesome-darkmode-toggle.js"
#        "${AWESOME_CSS_DIR}/doxygen-awesome-fragment-copy-button.js"
#        "${AWESOME_CSS_DIR}/doxygen-awesome-paragraph-link.js"
#        "${AWESOME_CSS_DIR}/doxygen-awesome-interactive-toc.js"
#    )
#
#    # Strip the project source directory from include paths in the documentation
#    set(DOXYGEN_STRIP_FROM_INC_PATH "${PROJECT_SOURCE_DIR}/include")
#    set(DOXYGEN_EXAMPLE_PATTERNS *)
#
#    execute_process(COMMAND doxygen -w html header.html footer.html style.css
#                    WORKING_DIRECTORY ${PROJECT_BINARY_DIR})
#    execute_process(COMMAND sed -i "/<\\/head>/r ${PROJECT_SOURCE_DIR}/cmake/extra_headers" header.html
#                    WORKING_DIRECTORY ${PROJECT_BINARY_DIR})
#    set(DOXYGEN_HTML_HEADER ${PROJECT_BINARY_DIR}/header.html)
#
#    # Generate Doxygen documentation
#    doxygen_add_docs(doxygen
#        "${CMAKE_CURRENT_SOURCE_DIR}/README.md"
#        "${CMAKE_CURRENT_SOURCE_DIR}/include" # Process headers in the include directory
#        "${CMAKE_CURRENT_SOURCE_DIR}/docs"
#        COMMENT "Generating Doxygen documentation"
#    )

#    file(GLOB DB_WRAP_EXAMPLE_FILES "${CMAKE_CURRENT_SOURCE_DIR}/examples/examples_*.cpp")
#    foreach(DB_WRAP_EXAMPLE_FILE ${DB_WRAP_EXAMPLE_FILES})
#        get_filename_component(DB_WRAP_EXAMPLE_NAME ${DB_WRAP_EXAMPLE_FILE} NAME_WE)
#        doxygen_add_example(doxygen ${DB_WRAP_EXAMPLE_FILE} "${DB_WRAP_EXAMPLE_NAME}")
#    endforeach()
#else()
#    message(WARNING "DBWRAP: Doxygen not found, documentation will not be generated.")
#endif()

# install targets
write_basic_package_version_file(
    ${DB_WRAP_CMAKE_PROJECT_CONFIG_FILE}
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

install(
    DIRECTORY ${DB_WRAP_INCLUDE_BUILD_DIR}
    DESTINATION ${DB_WRAP_INCLUDE_INSTALL_DIR}
)
install(
    FILES ${DB_WRAP_CMAKE_PROJECT_CONFIG_FILE} ${DB_WRAP_CMAKE_VERSION_CONFIG_FILE}
    DESTINATION ${DB_WRAP_CONFIG_INSTALL_DIR}
)

#export(
#    TARGETS ${DB_WRAP_TARGET_NAME}
#    NAMESPACE ${PROJECT_NAME}::
#    FILE ${DB_WRAP_CMAKE_PROJECT_TARGETS_FILE}
#)
#install(
#    TARGETS ${DB_WRAP_TARGET_NAME}
#    EXPORT ${DB_WRAP_TARGETS_EXPORT_NAME}
#    INCLUDES DESTINATION ${DB_WRAP_INCLUDE_INSTALL_DIR}
#)
#install(
#    EXPORT ${DB_WRAP_TARGETS_EXPORT_NAME}
#    NAMESPACE ${PROJECT_NAME}::
#    DESTINATION ${DB_WRAP_CONFIG_INSTALL_DIR}
#)
