set (SERVICE_LIB "indicatordisplayservice")
set (SERVICE_EXEC "indicator-display-service")

add_definitions (-DG_LOG_DOMAIN="${CMAKE_PROJECT_NAME}")

# handwritten source code...
set (SERVICE_LIB_HANDWRITTEN_SOURCES
     rotation-lock.cpp)

add_library (${SERVICE_LIB} STATIC
             ${SERVICE_LIB_HANDWRITTEN_SOURCES})

# add the bin dir to the include path so that
# the compiler can find the generated header files
include_directories (${CMAKE_CURRENT_BINARY_DIR})

link_directories (${SERVICE_DEPS_LIBRARY_DIRS})

set (SERVICE_EXEC_HANDWRITTEN_SOURCES main.cpp)
add_executable (${SERVICE_EXEC} ${SERVICE_EXEC_HANDWRITTEN_SOURCES})
target_link_libraries (${SERVICE_EXEC} ${SERVICE_LIB} ${SERVICE_DEPS_LIBRARIES} ${GCOV_LIBS})
install (TARGETS ${SERVICE_EXEC} RUNTIME DESTINATION ${CMAKE_INSTALL_FULL_PKGLIBEXECDIR})

# add warnings/coverage info on handwritten files
# but not the generated ones...
set_property (SOURCE ${SERVICE_LIB_HANDWRITTEN_SOURCES} ${SERVICE_EXEC_HANDWRITTEN_SOURCES}
              APPEND_STRING PROPERTY COMPILE_FLAGS " -std=c++11 -g ${CXX_WARNING_ARGS} ${GCOV_FLAGS}")

