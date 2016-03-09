
set(CMAKE_AUTOMOC ON)

find_package(GMock REQUIRED)
find_package(Qt5Core REQUIRED)
find_package(Qt5Test REQUIRED)
find_package(Qt5DBus COMPONENTS Qt5DBusMacros REQUIRED)

pkg_check_modules(TEST_DEPS
  libqtdbustest-1 REQUIRED
  libqtdbusmock-1 REQUIRED
)

include_directories(
  ${CMAKE_SOURCE_DIR}
  ${CMAKE_CURRENT_BINARY_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}
)
include_directories(SYSTEM
  ${DBUSTEST_INCLUDE_DIRS}
  ${TEST_DEPS_INCLUDE_DIRS}
  ${GTEST_INCLUDE_DIRS}
  ${GMOCK_INCLUDE_DIRS}
)

set(CTEST_ENVIRONMENT "${CTEST_ENVIRONMENT};G_MESSAGES_DEBUG=all")

if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
  # turn off the warnings that break Google Test
  set (CXX_WARNING_ARGS "${CXX_WARNING_ARGS} -Wno-global-constructors -Wno-weak-vtables")
endif()

SET (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIC -std=c++11 -g ${CXX_WARNING_ARGS}")

function(add_test_by_name name)
  set (TEST_NAME ${name})
  add_executable (${TEST_NAME} ${TEST_NAME}.cpp)
  add_test (${TEST_NAME} ${TEST_NAME})
  set_property(TEST ${TEST_NAME} PROPERTY ENVIRONMENT "${CTEST_ENVIRONMENT}")
  add_dependencies (${TEST_NAME} indicatordisplayservice)
  target_link_libraries (${TEST_NAME} indicatordisplayservice ${SERVICE_DEPS_LIBRARIES} ${TEST_DEPS_LIBRARIES} ${GTEST_LIBRARIES} ${GMOCK_LIBRARIES})
endfunction()
add_test_by_name(rotation-lock-test)
add_test_by_name(adbd-client-test)

function(add_qt_test_by_name name)
  set (TEST_NAME ${name})
  add_executable (${TEST_NAME} ${TEST_NAME}.cpp qmain.cpp)
  add_test (${TEST_NAME} ${TEST_NAME})
  set_property(TEST ${TEST_NAME} PROPERTY ENVIRONMENT "${CTEST_ENVIRONMENT}")
  add_dependencies (${TEST_NAME} indicatordisplayservice)
  target_link_libraries (${TEST_NAME} indicatordisplayservice ${SERVICE_DEPS_LIBRARIES} Qt5::Core Qt5::Test Qt5::DBus ${TEST_DEPS_LIBRARIES} ${GTEST_LIBRARIES} ${GMOCK_LIBRARIES})
endfunction()
add_qt_test_by_name(usb-snap-test)

add_test(cppcheck cppcheck --enable=all -USCHEMA_DIR --error-exitcode=2 --inline-suppr -I${CMAKE_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/tests)

