include_directories(
  ${CMAKE_CURRENT_BINARY_DIR} 
  ${CMAKE_CURRENT_SOURCE_DIR} 
)

add_library(
    test-utils
    STATIC
    qmain.cpp
)

qt5_use_modules(
    test-utils
    Core
    DBus
)

