###########################
# GSettings Schema
###########################

# build the necessary schemas
set_directory_properties (PROPERTIES
                          ADDITIONAL_MAKE_CLEAN_FILES gschemas.compiled)
set_source_files_properties (gschemas.compiled GENERATED)

# GSettings:
# compile the org.ayatana.display schema into a gschemas.compiled file in this directory,
# and help the tests to find that file by setting -DSCHEMA_DIR
set (XDG_DATA_DIRS "${CMAKE_CURRENT_BINARY_DIR}/gsettings-schemas")
set (SCHEMA_DIR "${XDG_DATA_DIRS}/glib-2.0/schemas")
set (DISPLAY_SCHEMA ${CMAKE_BINARY_DIR}/data/org.ayatana.display.gschema.xml)
add_definitions(-DSCHEMA_DIR="${SCHEMA_DIR}")
execute_process (COMMAND ${PKG_CONFIG_EXECUTABLE} gio-2.0 --variable glib_compile_schemas
                 OUTPUT_VARIABLE COMPILE_SCHEMA_EXECUTABLE
                 OUTPUT_STRIP_TRAILING_WHITESPACE)
# Because gschemas.compiled will be referenced multiple times independently, a level of
# redirection is needed to prevent CMake from generating the rule for the file multiple
# times, causing a race condition.
# https://discourse.cmake.org/t/race-condition-when-multi-add-custom-target-deps-on-same-file-generated-by-add-custom-command/2358
# https://bugs.debian.org/892091 (bug for ayatana-indicators-power, but applies here as well)
add_custom_command (OUTPUT gschemas.compiled
                    DEPENDS ${CMAKE_BINARY_DIR}/data/org.ayatana.display.gschema.xml
                    COMMAND mkdir -p ${SCHEMA_DIR}
                    COMMAND cp -f ${DISPLAY_SCHEMA} ${SCHEMA_DIR}
                    COMMAND ${COMPILE_SCHEMA_EXECUTABLE} ${SCHEMA_DIR})
add_custom_target(gschemas-compiled DEPENDS gschemas.compiled)

set(SERVICE_LINK_LIBRARIES
    ${SERVICE_LIB}
    ${SERVICE_DEPS_LIBRARIES}
)
set(QT_LINK_LIBRARIES
    test-utils
    Qt5::Core
    Qt5::Test
    Qt5::DBus
)
set(TEST_LINK_LIBRARIES
    ${TEST_DEPS_LIBRARIES}
    ${GTEST_LIBRARIES}
    ${GMOCK_LIBRARIES}
)

add_definitions(
  -DGREETER_TEMPLATE="${CMAKE_SOURCE_DIR}/tests/utils/mock-greeter.py"
)

function(add_test_by_name name)
  set(TEST_NAME ${name})
  set(COVERAGE_TEST_TARGETS ${COVERAGE_TEST_TARGETS} ${TEST_NAME} PARENT_SCOPE)
  add_executable (${TEST_NAME} ${TEST_NAME}.cpp)
  target_link_options(${TEST_NAME} PRIVATE -no-pie)
  add_test(${TEST_NAME} ${TEST_NAME})
  add_dependencies(${TEST_NAME} gschemas-compiled)
  set_property(TEST ${TEST_NAME} APPEND PROPERTY ENVIRONMENT ${CTEST_ENVIRONMENT})
  target_link_libraries(${TEST_NAME} ${SERVICE_LINK_LIBRARIES} ${TEST_LINK_LIBRARIES} ${THREAD_LINK_LIBRARIES})
endfunction()
add_test_by_name(adbd-client-test)
add_test_by_name(rotation-lock-test)

function(add_qt_test_by_name name)
  set(TEST_NAME ${name})
  set(COVERAGE_TEST_TARGETS ${COVERAGE_TEST_TARGETS} ${TEST_NAME} PARENT_SCOPE)
  add_executable (${TEST_NAME} ${TEST_NAME}.cpp)
  target_link_options(${TEST_NAME} PRIVATE -no-pie)
  add_test(${TEST_NAME} ${TEST_NAME})
  set_property(TEST ${TEST_NAME} APPEND PROPERTY ENVIRONMENT ${CTEST_ENVIRONMENT})
  target_link_libraries(${TEST_NAME} ${SERVICE_LINK_LIBRARIES} ${QT_LINK_LIBRARIES} ${TEST_LINK_LIBRARIES} ${THREAD_LINK_LIBRARIES})
endfunction()
add_qt_test_by_name(greeter-test)
add_qt_test_by_name(usb-snap-test)

set(COVERAGE_TEST_TARGETS
  ${COVERAGE_TEST_TARGETS}
  PARENT_SCOPE
)
