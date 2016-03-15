
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

if(${CMAKE_MAJOR_VERSION}.${CMAKE_MINOR_VERSION} LESS 3.2)
    set(SERVICE_THREAD_LIBS -pthread)
else()
    set(SERVICE_THREAD_LIBS Threads::Threads)
endif()

target_link_libraries(${SERVICE_EXEC}
    ${SERVICE_LIB}
    ${SERVICE_DEPS_LIBRARIES}
    ${SERVICE_THREAD_LIBS}
    ${GCOV_LIBS}
)

install(
    TARGETS
        ${SERVICE_EXEC}
    RUNTIME DESTINATION ${CMAKE_INSTALL_FULL_PKGLIBEXECDIR}
)

