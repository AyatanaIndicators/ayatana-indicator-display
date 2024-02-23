find_package(GSettings)

##
## GSettings schema
##

set (SCHEMA_NAME "org.ayatana.indicator.display.gschema.xml")
set (SCHEMA_FILE "${CMAKE_CURRENT_BINARY_DIR}/${SCHEMA_NAME}")

# let GSettings do the rest
find_package(GSettings REQUIRED)
add_schema (${SCHEMA_NAME})

##
##  Systemd Unit File
##

pkg_check_modules(SYSTEMD systemd)
if (${SYSTEMD_FOUND})
    # where to install
    pkg_get_variable(SYSTEMD_USER_DIR systemd systemduserunitdir)
    message (STATUS "${SYSTEMD_USER_DIR} is the systemd user unit file install dir")

    set (SYSTEMD_USER_NAME "${CMAKE_PROJECT_NAME}.service")
    set (SYSTEMD_USER_FILE "${CMAKE_CURRENT_BINARY_DIR}/${SYSTEMD_USER_NAME}")
    set (SYSTEMD_USER_FILE_IN "${CMAKE_CURRENT_SOURCE_DIR}/${SYSTEMD_USER_NAME}.in")

    # build it
    configure_file ("${SYSTEMD_USER_FILE_IN}" "${SYSTEMD_USER_FILE}")

    # install it
    install (FILES "${SYSTEMD_USER_FILE}"
             DESTINATION "${SYSTEMD_USER_DIR}")
endif()

##
##  XDG Autostart Config File
##

# build it
configure_file ("${CMAKE_CURRENT_SOURCE_DIR}/${CMAKE_PROJECT_NAME}.desktop.in" "${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_PROJECT_NAME}.desktop")

# install it
install (FILES "${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_PROJECT_NAME}.desktop" DESTINATION "/etc/xdg/autostart")

##
##  Ayatana Indicator File
##

# where to install
set (AYATANA_INDICATOR_DIR "${CMAKE_INSTALL_FULL_DATAROOTDIR}/ayatana/indicators")
message (STATUS "${AYATANA_INDICATOR_DIR} is the Ayatana Indicator install dir")

set (AYATANA_INDICATOR_NAME "org.ayatana.indicator.display")
set (AYATANA_INDICATOR_FILE "${CMAKE_CURRENT_SOURCE_DIR}/${AYATANA_INDICATOR_NAME}")

install (FILES "${AYATANA_INDICATOR_FILE}"
         DESTINATION "${AYATANA_INDICATOR_DIR}")


# ayatana-indicator-display-colortemp-off.svg
# ayatana-indicator-display-colortemp-on.svg
# ayatana-indicator-display-brightness-low
# ayatana-indicator-display-brightness-high

install (DIRECTORY "icons/" DESTINATION "${CMAKE_INSTALL_FULL_DATADIR}/icons/hicolor/scalable/status" FILES_MATCHING PATTERN "*.svg")

if (ENABLE_COLOR_TEMP)

    # org.ayatana.indicator.display.AccountsService.xml

    install (FILES org.ayatana.indicator.display.AccountsService.xml DESTINATION "${CMAKE_INSTALL_FULL_DATADIR}/dbus-1/interfaces")
    install (DIRECTORY DESTINATION "${CMAKE_INSTALL_FULL_DATADIR}/accountsservice/interfaces/")
    install (CODE "message(STATUS \"Symlinking: \$ENV{DESTDIR}${CMAKE_INSTALL_FULL_DATADIR}/accountsservice/interfaces/org.ayatana.indicator.display.AccountsService.xml\")")
    install (CODE "execute_process(COMMAND ${CMAKE_COMMAND} -E create_symlink ../../dbus-1/interfaces/org.ayatana.indicator.display.AccountsService.xml \$ENV{DESTDIR}${CMAKE_INSTALL_FULL_DATADIR}/accountsservice/interfaces/org.ayatana.indicator.display.AccountsService.xml)")

    # org.ayatana.indicator.display.AccountsService.policy

    install (FILES org.ayatana.indicator.display.AccountsService.policy DESTINATION "${CMAKE_INSTALL_FULL_DATADIR}/polkit-1/actions")

    # 50-org.ayatana.indicator.display.AccountsService.rules

    install (FILES 50-org.ayatana.indicator.display.AccountsService.rules DESTINATION "${CMAKE_INSTALL_FULL_DATADIR}/polkit-1/rules.d")

    # 50-org.ayatana.indicator.display.AccountsService.pkla

    install (FILES 50-org.ayatana.indicator.display.AccountsService.pkla DESTINATION "${CMAKE_INSTALL_FULL_LOCALSTATEDIR}/lib/polkit-1/localauthority/10-vendor.d")

endif ()
