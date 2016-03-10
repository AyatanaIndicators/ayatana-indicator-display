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

function(add_qt_test_by_name name)
  set(TEST_NAME ${name})
  add_executable (${TEST_NAME} ${TEST_NAME}.cpp)
  add_test(${TEST_NAME} ${TEST_NAME})
  set_property(TEST ${TEST_NAME} APPEND PROPERTY ENVIRONMENT "${CTEST_ENVIRONMENT}")
  target_link_libraries(${TEST_NAME} ${SERVICE_LINK_LIBRARIES} ${QT_LINK_LIBRARIES} ${TEST_LINK_LIBRARIES})
endfunction()
add_qt_test_by_name(usb-manager-test)
