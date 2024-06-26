set(CMAKE_AUTOMOC ON)

find_package(GMock REQUIRED)
find_package(Qt5Test REQUIRED)
find_package(Qt5DBus COMPONENTS Qt5DBusMacros REQUIRED)

pkg_check_modules(TEST_DEPS
    libqtdbustest-1 REQUIRED
    libqtdbusmock-1 REQUIRED
)

include_directories(SYSTEM
    ${DBUSTEST_INCLUDE_DIRS}
    ${TEST_DEPS_INCLUDE_DIRS}
    ${GTEST_INCLUDE_DIRS}
    ${GMOCK_INCLUDE_DIRS}
)

list(APPEND CTEST_ENVIRONMENT
    G_MESSAGES_DEBUG=all
    G_DBUS_DEBUG=call,signal,return,message
)

# turn off the warnings that break Google Test
if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
    list(APPEND CXX_WARNING_ARGS -Wno-global-constructors -Wno-weak-vtables)
endif()

add_compile_options(${CXX_WARNING_ARGS})

add_test(cppcheck cppcheck --enable=all -USCHEMA_DIR --check-level=exhaustive --error-exitcode=2 --inline-suppr --library=qt -I${CMAKE_SOURCE_DIR} -i${CMAKE_SOURCE_DIR}/tests/utils/qmain.cpp -i${CMAKE_SOURCE_DIR}/tests/gmock ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/tests --suppress=missingIncludeSystem --suppress=uninitDerivedMemberVar --suppress=unmatchedSuppression --suppress=constParameter --suppress=constParameterCallback --suppress=unusedFunction --suppress=uselessOverride)

add_subdirectory (unit)

set(COVERAGE_TEST_TARGETS
  ${COVERAGE_TEST_TARGETS}
  PARENT_SCOPE
)
