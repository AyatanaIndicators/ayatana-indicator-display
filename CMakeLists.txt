project(indicator-display LANGUAGES C CXX)
cmake_minimum_required(VERSION 2.8.9)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake)

set(PACKAGE ${CMAKE_PROJECT_NAME})
set(GETTEXT_PACKAGE indicator-display)
add_definitions(
    -DGETTEXT_PACKAGE="${GETTEXT_PACKAGE}"
    -DGNOMELOCALEDIR="${CMAKE_INSTALL_FULL_LOCALEDIR}"
)

set(SERVICE_LIB ${PACKAGE})
set(SERVICE_EXEC "${PACKAGE}-service")

##
##  GNU standard paths
## 

include(GNUInstallDirs)
if(EXISTS "/etc/debian_version") # Workaround for libexecdir on debian
    set(CMAKE_INSTALL_LIBEXECDIR "${CMAKE_INSTALL_LIBDIR}")
    set(CMAKE_INSTALL_FULL_LIBEXECDIR "${CMAKE_INSTALL_FULL_LIBDIR}")
endif()
set(CMAKE_INSTALL_PKGLIBEXECDIR "${CMAKE_INSTALL_LIBEXECDIR}/${CMAKE_PROJECT_NAME}")
set(CMAKE_INSTALL_FULL_PKGLIBEXECDIR "${CMAKE_INSTALL_FULL_LIBEXECDIR}/${CMAKE_PROJECT_NAME}")

##
##  Check for prerequisites
##

# threads...
set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)
if(${CMAKE_MAJOR_VERSION}.${CMAKE_MINOR_VERSION} LESS 3.1)
    set(THREAD_LINK_LIBRARIES -pthread)
else()
    set(THREAD_LINK_LIBRARIES Threads::Threads) # introduced in cmake 3.1
endif()

find_package(PkgConfig REQUIRED)

# glib...
set(GLIB_MINIMUM 2.36)
pkg_check_modules(SERVICE_DEPS REQUIRED
    gio-unix-2.0>=${GLIB_MINIMUM}
    glib-2.0>=${GLIB_MINIMUM}
    gudev-1.0
)
include_directories (SYSTEM
  ${SERVICE_DEPS_INCLUDE_DIRS}
)

##
##  Compiler settings
##

include_directories(${CMAKE_CURRENT_SOURCE_DIR})

# set the compiler warnings
if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
    list(APPEND CXX_WARNING_ARGS -Weverything -Wno-c++98-compat -Wno-padded)
elseif("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
    list(APPEND CXX_WARNING_ARGS -Wall -Wextra -Wpedantic)
endif()

add_compile_options(-std=c++14 -fPIC -g)

add_subdirectory(src)
add_subdirectory(data)
add_subdirectory(po)

##
##  Testing & Coverage
##

enable_testing()
add_subdirectory(tests)
find_package(CoverageReport)
ENABLE_COVERAGE_REPORT(
  TARGETS ${SERVICE_LIB} ${SERVICE_EXEC}
  TESTS ${COVERAGE_TEST_TARGETS}
  FILTER /usr/include ${CMAKE_BINARY_DIR}/*
)

