find_package(GSettings)

##
##  Upstart Job File
##

# where to install
set (UPSTART_JOBS_DIR "${CMAKE_INSTALL_FULL_DATADIR}/upstart/sessions")
message (STATUS "${UPSTART_JOBS_DIR} is the Upstart Jobs File install dir")

set (UPSTART_JOB_NAME "${CMAKE_PROJECT_NAME}.conf")
set (UPSTART_JOB_FILE "${CMAKE_CURRENT_BINARY_DIR}/${UPSTART_JOB_NAME}")
set (UPSTART_JOB_FILE_IN "${CMAKE_CURRENT_SOURCE_DIR}/${UPSTART_JOB_NAME}.in")

# build it
set (pkglibexecdir "${CMAKE_INSTALL_FULL_PKGLIBEXECDIR}")
configure_file ("${UPSTART_JOB_FILE_IN}" "${UPSTART_JOB_FILE}")

# install it
install (FILES "${UPSTART_JOB_FILE}"
         DESTINATION "${UPSTART_JOBS_DIR}")

##
## GSettings
##

if(NOT EXISTS /usr/share/glib-2.0/schemas/com.ubuntu.touch.system.gschema.xml)
  add_schema ("org.ayatana.display.gschema.xml")
endif()

##
##  Systemd Unit File
##

# where to install
pkg_get_variable(SYSTEMD_USER_DIR systemd systemduserunitdir)
message (STATUS "${SYSTEMD_USER_DIR} is the systemd user unit file install dir")

set (SYSTEMD_USER_NAME "${CMAKE_PROJECT_NAME}.service")
set (SYSTEMD_USER_FILE "${CMAKE_CURRENT_BINARY_DIR}/${SYSTEMD_USER_NAME}")
set (SYSTEMD_USER_FILE_IN "${CMAKE_CURRENT_SOURCE_DIR}/${SYSTEMD_USER_NAME}.in")

# build it
set (pkglibexecdir "${CMAKE_INSTALL_FULL_PKGLIBEXECDIR}")
configure_file ("${SYSTEMD_USER_FILE_IN}" "${SYSTEMD_USER_FILE}")

# install it
install (FILES "${SYSTEMD_USER_FILE}"
         DESTINATION "${SYSTEMD_USER_DIR}")

##
##  Upstart systemd override Job File
##

set (UPSTART_SYSTEMD_OVERRIDE_DIR "${CMAKE_INSTALL_FULL_DATADIR}/upstart/systemd-session/upstart")
message (STATUS "${UPSTART_SYSTEMD_OVERRIDE_DIR} is the Upstart override Job File for systemd dir")

install (FILES "${CMAKE_CURRENT_SOURCE_DIR}/${CMAKE_PROJECT_NAME}.override"
         DESTINATION "${UPSTART_SYSTEMD_OVERRIDE_DIR}")

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
