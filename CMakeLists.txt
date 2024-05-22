cmake_minimum_required(VERSION 3.13)
project(ayatana-indicator-display VERSION 24.5.0 LANGUAGES C CXX)

if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    SET(CMAKE_INSTALL_PREFIX "/usr" CACHE PATH "..." FORCE)
endif(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)

##
##  GNU standard paths
##
include(GNUInstallDirs)

set(PACKAGE ${CMAKE_PROJECT_NAME})
set(SERVICE_LIB ${PACKAGE})
set(SERVICE_EXEC "${PACKAGE}-service")

# Options

option(ENABLE_TESTS "Enable all tests and checks" OFF)
option(ENABLE_COVERAGE "Enable coverage reports (includes enabling all tests and checks)" OFF)
option(ENABLE_WERROR "Treat all build warnings as errors" OFF)
option(ENABLE_COLOR_TEMP "Include colour temperature specific code in the build" ON)

if(ENABLE_COVERAGE)
    set(ENABLE_TESTS ON)
    set(CMAKE_BUILD_TYPE "Coverage")
else()
    set(CMAKE_BUILD_TYPE "Release")
endif()

if(ENABLE_WERROR)
    add_definitions("-Werror")
endif()

if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
    add_definitions("-Weverything")
elseif("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
    add_definitions("-Wall")
endif()

##
## Gettext
##

set(GETTEXT_PACKAGE "ayatana-indicator-display")
add_definitions(
    -DGETTEXT_PACKAGE="${GETTEXT_PACKAGE}"
    -DLOCALEDIR="${CMAKE_INSTALL_FULL_LOCALEDIR}"
)

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
set (SERVICE_DEPS libayatana-common>=0.9.3 gio-unix-2.0>=${GLIB_MINIMUM} glib-2.0>=${GLIB_MINIMUM} properties-cpp>=0.0.1)

if (ENABLE_COLOR_TEMP)
    list (APPEND SERVICE_DEPS libgeoclue-2.0 accountsservice)
    add_definitions (-DCOLOR_TEMP_ENABLED)
endif ()

pkg_check_modules (SERVICE_DEPS REQUIRED ${SERVICE_DEPS})

include_directories (SYSTEM
  ${SERVICE_DEPS_INCLUDE_DIRS}
)

##
##  Compiler settings
##

include_directories(${CMAKE_CURRENT_SOURCE_DIR})
add_compile_options(-fPIC)

add_subdirectory(src)
add_subdirectory(data)
add_subdirectory(po)

##
##  Testing & Coverage
##

if (ENABLE_TESTS)

    include(CTest)
    enable_testing()
    add_subdirectory(tests)

    if (ENABLE_COVERAGE)
        find_package(CoverageReport)
        ENABLE_COVERAGE_REPORT(
            TARGETS ${SERVICE_LIB} ${SERVICE_EXEC}
            TESTS ${COVERAGE_TEST_TARGETS}
            FILTER /usr/include ${CMAKE_BINARY_DIR}/*
        )

    endif()

endif()

# Display config info

message(STATUS "Install prefix: ${CMAKE_INSTALL_PREFIX}")
message(STATUS "Unit tests: ${ENABLE_TESTS}")
message(STATUS "Build with -Werror: ${ENABLE_WERROR}")
message(STATUS "Build with colour temperature code: ${ENABLE_COLOR_TEMP}")
