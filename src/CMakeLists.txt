
add_definitions(-DG_LOG_DOMAIN="${CMAKE_PROJECT_NAME}")

add_compile_options(
  ${CXX_WARNING_ARGS}
  ${GCOV_FLAGS}
)

add_library(
    ${SERVICE_LIB}
    STATIC
    adbd-client.cpp
    exporter.cpp
    indicator.cpp
    rotation-lock.cpp
    usb-manager.cpp
    usb-snap.cpp
)

add_executable(
    ${SERVICE_EXEC}
    main.cpp
)

target_link_libraries(${SERVICE_EXEC}
    ${SERVICE_LIB}
    ${SERVICE_DEPS_LIBRARIES}
    ${THREAD_LINK_LIBRARIES}
    ${GCOV_LIBS}
)

install(
    TARGETS
        ${SERVICE_EXEC}
    RUNTIME DESTINATION ${CMAKE_INSTALL_FULL_PKGLIBEXECDIR}
)

