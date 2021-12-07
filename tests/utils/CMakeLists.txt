include_directories(
  ${CMAKE_CURRENT_BINARY_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}
)

add_library(
    test-utils
    STATIC
    qmain.cpp
)

target_link_libraries(
    test-utils
    Qt5::Core
    Qt5::DBus
)
