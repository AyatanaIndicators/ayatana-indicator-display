project (indicator-display)
cmake_minimum_required (VERSION 2.8.9)

list (APPEND CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake)

set (PROJECT_VERSION "14.10.0")
set (PACKAGE ${CMAKE_PROJECT_NAME})
set (GETTEXT_PACKAGE indicator-display)

set (SERVICE_LIB ${PACKAGE})
set (SERVICE_EXEC "${PACKAGE}-service")

option (enable_tests "Build the package's automatic tests." ON)
option (enable_lcov "Generate lcov code coverage reports." ON)

##
##  GNU standard paths
## 
include (GNUInstallDirs)
if (EXISTS "/etc/debian_version") # Workaround for libexecdir on debian
  set (CMAKE_INSTALL_LIBEXECDIR "${CMAKE_INSTALL_LIBDIR}")
  set (CMAKE_INSTALL_FULL_LIBEXECDIR "${CMAKE_INSTALL_FULL_LIBDIR}")
endif ()
set (CMAKE_INSTALL_PKGLIBEXECDIR "${CMAKE_INSTALL_LIBEXECDIR}/${CMAKE_PROJECT_NAME}")
set (CMAKE_INSTALL_FULL_PKGLIBEXECDIR "${CMAKE_INSTALL_FULL_LIBEXECDIR}/${CMAKE_PROJECT_NAME}")

##
##  Check for prerequisites
##

find_package (PkgConfig REQUIRED)

include (FindPkgConfig)
pkg_check_modules (SERVICE_DEPS REQUIRED
                   url-dispatcher-1
                   gio-unix-2.0>=2.36
                   glib-2.0>=2.36)
include_directories (SYSTEM ${SERVICE_DEPS_INCLUDE_DIRS})

##
##
##

set (CMAKE_INCLUDE_CURRENT_DIR ON)
include_directories (${CMAKE_CURRENT_SOURCE_DIR}/include)
include_directories (${CMAKE_CURRENT_BINARY_DIR}/include)

# set the compiler warnings
if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
  set (CXX_WARNING_ARGS "${CXX_WARNING_ARGS} -Weverything -Wno-c++98-compat")
else()
  set (CXX_WARNING_ARGS "${CXX_WARNING_ARGS} -Wall -Wextra -Wpedantic")
endif()
set (CXX_WARNING_ARGS "${CXX_WARNING_ARGS} -Wno-missing-field-initializers") # GActionEntry

# testing & coverage
if (${enable_tests})
  set (GTEST_SOURCE_DIR /usr/src/gtest/src)
  set (GTEST_INCLUDE_DIR ${GTEST_SOURCE_DIR}/..)
  set (GTEST_LIBS -lpthread)
  enable_testing ()
  if (${enable_lcov})
    include(GCov)
  endif ()
endif ()

add_subdirectory (src)
add_subdirectory (data)
add_subdirectory (po)
if (${enable_tests})
  add_subdirectory (tests)
endif ()
