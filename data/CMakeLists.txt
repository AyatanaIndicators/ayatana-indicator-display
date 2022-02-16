find_package(GSettings)

##
## GSettings schema
##

set (SCHEMA_NAME "org.ayatana.display.gschema.xml")
set (SCHEMA_FILE "${CMAKE_CURRENT_BINARY_DIR}/${SCHEMA_NAME}")
set (SCHEMA_FILE_IN "${CMAKE_CURRENT_SOURCE_DIR}/${SCHEMA_NAME}.in")

# generate the .xml file using intltool
find_package(Intltool REQUIRED)
set (ENV{LC_ALL} "C")
intltool_merge_translations("${SCHEMA_FILE_IN}" "${SCHEMA_FILE}" ALL UTF8 STYLE "xml" NO_TRANSLATIONS)

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

set (AYATANA_INDICATOR_NAME "org.ayatana.indicator.rotation_lock")
set (AYATANA_INDICATOR_FILE "${CMAKE_CURRENT_SOURCE_DIR}/${AYATANA_INDICATOR_NAME}")

install (FILES "${AYATANA_INDICATOR_FILE}"
         DESTINATION "${AYATANA_INDICATOR_DIR}")
