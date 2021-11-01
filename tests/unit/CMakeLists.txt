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
if (EXISTS /usr/share/glib-2.0/schemas/com.lomiri.touch.system.gschema.xml)
  set (DISPLAY_SCHEMA /usr/share/glib-2.0/schemas/com.lomiri.touch.system.gschema.xml)
else()
  set (DISPLAY_SCHEMA ${CMAKE_SOURCE_DIR}/data/org.ayatana.display.gschema.xml)
endif()
add_definitions(-DSCHEMA_DIR="${SCHEMA_DIR}")
execute_process (COMMAND ${PKG_CONFIG_EXECUTABLE} gio-2.0 --variable glib_compile_schemas
                 OUTPUT_VARIABLE COMPILE_SCHEMA_EXECUTABLE
                 OUTPUT_STRIP_TRAILING_WHITESPACE)
add_custom_command (OUTPUT gschemas.compiled
                    COMMAND mkdir -p ${SCHEMA_DIR}
                    COMMAND cp -f ${DISPLAY_SCHEMA} ${SCHEMA_DIR}
                    COMMAND ${COMPILE_SCHEMA_EXECUTABLE} ${SCHEMA_DIR})

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
  add_executable (${TEST_NAME} ${TEST_NAME}.cpp gschemas.compiled)
  target_link_options(${TEST_NAME} PRIVATE -no-pie)
  add_test(${TEST_NAME} ${TEST_NAME})
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
