add_definitions(-DG_LOG_DOMAIN="${CMAKE_PROJECT_NAME}")

if(EXISTS "/usr/share/glib-2.0/schemas/com.lomiri.touch.system.gschema.xml")
  add_definitions( -DHAS_LOMIRI_TOUCH_SCHEMA )
endif()

add_compile_options(
  ${CXX_WARNING_ARGS}
)

add_library(
    ${SERVICE_LIB}
    STATIC
    adbd-client.cpp
    exporter.cpp
    greeter.cpp
    indicator.cpp
    rotation-lock.cpp
    usb-manager.cpp
    usb-monitor.cpp
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
)

install(
    TARGETS
        ${SERVICE_EXEC}
    RUNTIME DESTINATION ${CMAKE_INSTALL_FULL_PKGLIBEXECDIR}
)
