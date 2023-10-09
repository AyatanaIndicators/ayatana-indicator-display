add_definitions(-DG_LOG_DOMAIN="${CMAKE_PROJECT_NAME}")

add_compile_options(
  ${CXX_WARNING_ARGS}
)

set (SERVICE_LIB_SOURCES
     exporter.cpp
     indicator.cpp
     service.cpp
)

if (ENABLE_LOMIRI_FEATURES)
    list (APPEND
          SERVICE_LIB_SOURCES
          greeter.cpp
          adbd-client.cpp
          usb-manager.cpp
          usb-monitor.cpp
          usb-snap.cpp
    )
endif ()

if (ENABLE_COLOR_TEMP)
    list (APPEND
          SERVICE_LIB_SOURCES
          solar.c
    )
endif ()

add_library (${SERVICE_LIB} STATIC ${SERVICE_LIB_SOURCES})

if (ENABLE_LOMIRI_FEATURES)
    target_link_libraries(${SERVICE_LIB} Qt5::Core)
endif ()

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
    RUNTIME DESTINATION "${CMAKE_INSTALL_FULL_LIBEXECDIR}/${CMAKE_PROJECT_NAME}"
)
