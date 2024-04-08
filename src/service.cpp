/*
 * Copyright 2014 Canonical Ltd.
 * Copyright 2023-2024 Robert Tari
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors:
 *   Charles Kerr <charles.kerr@canonical.com>
 *   Robert Tari <robert@tari.in>
 */

#include <src/service.h>
#include <glib/gi18n.h>

#ifdef COLOR_TEMP_ENABLED
    #include <geoclue.h>
#endif

extern "C"
{
    #include <ayatana/common/utils.h>

    #ifdef COLOR_TEMP_ENABLED
        #include <act/act.h>
        #include <pwd.h>
        #include "solar.h"
    #endif
}

#ifdef COLOR_TEMP_ENABLED
    #define GREETER_BUS_NAME "org.ayatana.greeter"
    #define GREETER_BUS_PATH "/org/ayatana/greeter"

    typedef struct
    {
       guint nTempLow;
       guint nTempHigh;
       const gchar *sName;
    } TempProfile;

    TempProfile m_lTempProfiles[] =
    {
        {0, 0, N_("Manual")},
        {4500, 6500, N_("Adaptive (Colder)")},
        {3627, 4913, N_("Adaptive")},
        {3058, 4913, N_("Adaptive (Warmer)")},
        {0, 0, NULL}
    };
#endif

class DisplayIndicator::Impl
{
public:

  Impl()
  {
#ifdef COLOR_TEMP_ENABLED
    const gchar *sTest = g_getenv ("TEST_NAME");
    this->bTest = (sTest != NULL && g_str_equal (sTest, "rotation-lock-test"));
#endif
    const char *sUserName = g_get_user_name();
    this->bGreeter = g_str_equal (sUserName, "lightdm");
    GSettingsSchemaSource *pSource = g_settings_schema_source_get_default();

    if (pSource != NULL)
    {
        if (ayatana_common_utils_is_lomiri()) {

            GSettingsSchema *pSchema = g_settings_schema_source_lookup(pSource, "com.lomiri.touch.system", FALSE);

            if (pSchema != NULL)
            {
                g_settings_schema_unref(pSchema);
                m_settings = g_settings_new("com.lomiri.touch.system");
            }
            else
            {
                g_error("No schema could be found");
            }

        }
        else {

            GSettingsSchema *pSchema = g_settings_schema_source_lookup(pSource, "org.ayatana.indicator.display", FALSE);

            if (pSchema != NULL)
            {
                g_settings_schema_unref(pSchema);
                m_settings = g_settings_new("org.ayatana.indicator.display");
            }
            else
            {
                g_error("No schema could be found");
            }

#ifdef COLOR_TEMP_ENABLED
            this->pAccountsServiceConnection = g_bus_get_sync (G_BUS_TYPE_SYSTEM, NULL, NULL);

            if (!this->bGreeter)
            {
                const gchar *sSchema = NULL;
                const gchar *sCursorSchema = NULL;
                const gchar *sMetacitySchema = NULL;

                if (this->bTest)
                {
                    sSchema = "org.ayatana.indicator.display";
                    sCursorSchema = "org.ayatana.indicator.display";
                    sMetacitySchema = "org.ayatana.indicator.display";
                }
                else
                {
                    if (ayatana_common_utils_is_mate ())
                    {
                        sSchema = "org.mate.interface";
                        sCursorSchema = "org.mate.peripherals-mouse";
                        sMetacitySchema = "org.mate.Marco.general";
                    }
                    else
                    {
                        sSchema = "org.gnome.desktop.interface";
                        sCursorSchema = "org.gnome.desktop.interface";
                        sMetacitySchema = "org.gnome.desktop.wm.preferences";
                    }
                }

                pSchema = g_settings_schema_source_lookup (pSource, sSchema, FALSE);

                if (pSchema != NULL)
                {
                    g_settings_schema_unref (pSchema);
                    pThemeSettings = g_settings_new (sSchema);
                }
                else
                {
                    g_error("No %s schema could be found", sSchema);
                }

                pSchema = g_settings_schema_source_lookup (pSource, sCursorSchema, FALSE);

                if (pSchema != NULL)
                {
                    g_settings_schema_unref (pSchema);
                    pCursorSettings = g_settings_new (sCursorSchema);
                }
                else
                {
                    g_error("No %s schema could be found", sCursorSchema);
                }

                pSchema = g_settings_schema_source_lookup (pSource, sMetacitySchema, FALSE);

                if (pSchema != NULL)
                {
                    g_settings_schema_unref (pSchema);
                    pMetacitySettings = g_settings_new (sMetacitySchema);
                }
                else
                {
                    g_error("No %s schema could be found", sMetacitySchema);
                }

                if (this->bTest)
                {
                    sSchema = "org.ayatana.indicator.display";
                }
                else
                {
                    sSchema = "org.gnome.desktop.interface";
                }

                pSchema = g_settings_schema_source_lookup (pSource, sSchema, FALSE);

                if (pSchema != NULL)
                {
                    gboolean bColorScheme = g_settings_schema_has_key (pSchema, "color-scheme");
                    g_settings_schema_unref (pSchema);

                    if (bColorScheme)
                    {
                        pColorSchemeSettings = g_settings_new (sSchema);
                    }
                    else
                    {
                        g_warning ("org.gnome.desktop.interface::color-scheme not found. Native theme profile changes will not be triggered.");
                    }
                }
                else
                {
                    g_error("No %s schema could be found", sSchema);
                }
            }
            else
            {
                this->pConnection = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, NULL);
                this->nGreeterSubscription = g_dbus_connection_signal_subscribe (this->pConnection, NULL, GREETER_BUS_NAME, "UserChanged", GREETER_BUS_PATH, NULL, G_DBUS_SIGNAL_FLAGS_NONE, onUserChanged, this, NULL);
                loadManager (this);
            }
#endif
        }
    }

#ifdef COLOR_TEMP_ENABLED
    if (!this->bGreeter)
    {
        gint nUid = geteuid ();
        getAccountsService (this, nUid);
    }
#endif
    m_action_group = create_action_group();

    // build the icon
    const char *icon_name {"orientation-lock"};

    if (!ayatana_common_utils_is_lomiri())
    {
        icon_name = "display-panel";
    }

    auto icon = g_themed_icon_new_with_default_fallbacks(icon_name);
    auto icon_deleter = [](GIcon* o){g_object_unref(G_OBJECT(o));};
    m_icon.reset(icon, icon_deleter);

    // build the phone profile
    auto menu_model_deleter = [](GMenuModel* o){g_object_unref(G_OBJECT(o));};
    std::shared_ptr<GMenuModel> phone_menu (create_phone_menu(), menu_model_deleter);
    m_phone = std::make_shared<SimpleProfile>("phone", phone_menu);
    update_phone_header();

    // build the desktop profile
    std::shared_ptr<GMenuModel> desktop_menu (create_desktop_menu(), menu_model_deleter);
    m_desktop = std::make_shared<SimpleProfile>("desktop", desktop_menu);
    update_desktop_header();

#ifdef COLOR_TEMP_ENABLED
    if (ayatana_common_utils_is_lomiri() == FALSE)
    {
        if (!this->bTest)
        {
            this->fLatitude = g_settings_get_double (this->m_settings, "latitude");
            this->fLongitude = g_settings_get_double (this->m_settings, "longitude");
            gclue_simple_new ("ayatana-indicator-display", GCLUE_ACCURACY_LEVEL_CITY, NULL, onGeoClueLoaded, this);
            this->nCallback = g_timeout_add_seconds (60, updateColor, this);
            updateColor (this);
        }
    }
#endif
  }

  ~Impl()
  {
#ifdef COLOR_TEMP_ENABLED
    if (this->nGreeterSubscription)
    {
        g_dbus_connection_signal_unsubscribe (this->pConnection, this->nGreeterSubscription);
    }

    if (this->lUsers)
    {
        g_slist_free (this->lUsers);
    }

    if (this->pConnection)
    {
        g_object_unref (this->pConnection);
    }

    if (nCallback)
    {
        g_source_remove (nCallback);
    }

    if (sLastTheme)
    {
        g_free (sLastTheme);
    }

    if (pThemeSettings)
    {
        g_clear_object (&pThemeSettings);
    }

    if (pCursorSettings)
    {
        g_clear_object (&pCursorSettings);
    }

    if (pMetacitySettings)
    {
        g_clear_object (&pMetacitySettings);
    }

    if (pColorSchemeSettings)
    {
        g_clear_object (&pColorSchemeSettings);
    }

    if (this->pAccountsServiceConnection)
    {
        g_object_unref (this->pAccountsServiceConnection);
    }
#endif

    g_signal_handlers_disconnect_by_data(m_settings, this);
    g_clear_object(&m_action_group);
    g_clear_object(&m_settings);
  }

  GSimpleActionGroup* action_group() const
  {
    return m_action_group;
  }

  std::vector<std::shared_ptr<Profile>> profiles()
  {
    std::vector<std::shared_ptr<Profile>> ret;
    ret.push_back(m_phone);
    ret.push_back(m_desktop);
    return ret;
  }

private:

#ifdef COLOR_TEMP_ENABLED
    static void onUserChanged (GDBusConnection *pConnection, const gchar *sSender, const gchar *sPath, const gchar *sInterface, const gchar *sSignal, GVariant *pParameters, gpointer pUserData)
    {
        DisplayIndicator::Impl *pImpl = (DisplayIndicator::Impl*) pUserData;
        g_variant_get (pParameters, "(s)", &pImpl->sUser);
        loadManager (pImpl);
    }

    static void getAccountsService (DisplayIndicator::Impl *pImpl, gint nUid)
    {
        pImpl->bReadingAccountsService = TRUE;
        gchar *sPath = g_strdup_printf ("/org/freedesktop/Accounts/User%i", nUid);
        GDBusProxy *pProxy = g_dbus_proxy_new_sync (pImpl->pAccountsServiceConnection, G_DBUS_PROXY_FLAGS_NONE, NULL, "org.freedesktop.Accounts", sPath, "org.freedesktop.DBus.Properties", NULL, NULL);
        g_free (sPath);

        if (pProxy)
        {
            const gchar *lProperties[] = {"brightness", "color-temp", "color-temp-profile"};

            for (gint nIndex = 0; nIndex < 3; nIndex++)
            {
                GVariant *pParams = g_variant_new ("(ss)", "org.ayatana.indicator.display.AccountsService", lProperties[nIndex]);
                GVariant *pValue = g_dbus_proxy_call_sync (pProxy, "Get", pParams, G_DBUS_CALL_FLAGS_NONE, -1, NULL, NULL);

                if (pValue)
                {
                    GVariant *pChild0 = g_variant_get_child_value (pValue, 0);
                    g_variant_unref (pValue);
                    GVariant *pChild1 = g_variant_get_child_value (pChild0, 0);
                    g_variant_unref (pChild0);
                    g_settings_set_value (pImpl->m_settings, lProperties[nIndex], pChild1);
                    g_variant_unref (pChild1);
                }
            }
        }

        pImpl->bReadingAccountsService = FALSE;
    }

    static void onUserLoaded (DisplayIndicator::Impl *pImpl, ActUser *pUser)
    {
        g_signal_handlers_disconnect_by_func (G_OBJECT (pUser), (gpointer) G_CALLBACK (onUserLoaded), pImpl);

        if (!pImpl->sUser)
        {
            GError *pError = NULL;
            GVariant *pGreeterUser = g_dbus_connection_call_sync (pImpl->pConnection, GREETER_BUS_NAME, GREETER_BUS_PATH, GREETER_BUS_NAME, "GetUser", NULL, G_VARIANT_TYPE ("(s)"), G_DBUS_CALL_FLAGS_NONE, -1, NULL, &pError);

            if (pError)
            {
                g_debug ("Failed calling GetUser, the greeter may not be ready yet: %s", pError->message);
                g_error_free (pError);

                return;
            }

            g_variant_get (pGreeterUser, "(s)", &pImpl->sUser);
        }

        gboolean bPrefix = g_str_has_prefix (pImpl->sUser, "*");

        if (!bPrefix)
        {
            const gchar *sUserName = act_user_get_user_name (pUser);
            gboolean bSame = g_str_equal (pImpl->sUser, sUserName);

            if (bSame)
            {
                gint nUid = act_user_get_uid (pUser);
                getAccountsService (pImpl, nUid);
                updateColor (pImpl);
            }
        }
    }

    static void onManagerLoaded (DisplayIndicator::Impl *pImpl)
    {
        ActUserManager *pManager = act_user_manager_get_default ();

        if (!pImpl->lUsers)
        {
            pImpl->lUsers = act_user_manager_list_users (pManager);
        }

        for (GSList *lUser = pImpl->lUsers; lUser; lUser = lUser->next)
        {
            ActUser *pUser = static_cast<ActUser*>(lUser->data);
            gboolean bLoaded = act_user_is_loaded (pUser);

            if (bLoaded)
            {
                onUserLoaded (pImpl, pUser);
            }
            else
            {
                g_signal_connect_swapped (pUser, "notify::is-loaded", G_CALLBACK (onUserLoaded), pImpl);
            }
        }
    }

    static void loadManager (DisplayIndicator::Impl *pImpl)
    {
        ActUserManager *pManager = act_user_manager_get_default ();
        gboolean bLoaded = FALSE;
        g_object_get (pManager, "is-loaded", &bLoaded, NULL);

        if (bLoaded)
        {
            onManagerLoaded (pImpl);
        }
        else
        {
            g_signal_connect_swapped (pManager, "notify::is-loaded", G_CALLBACK (onManagerLoaded), pImpl);
        }
    }

    static gboolean updateColor (gpointer pData)
    {
        DisplayIndicator::Impl *pImpl = (DisplayIndicator::Impl*) pData;

        if (pImpl->bReadingAccountsService)
        {
            return G_SOURCE_CONTINUE;
        }

        guint nProfile = 0;
        g_settings_get (pImpl->m_settings, "color-temp-profile", "q", &nProfile);
        gdouble fBrightness = g_settings_get_double (pImpl->m_settings, "brightness");
        gchar *sThemeProfile = NULL;
        gboolean bThemeAdaptive = FALSE;

        if (!pImpl->bGreeter)
        {
            sThemeProfile = g_settings_get_string (pImpl->m_settings, "theme-profile");
            bThemeAdaptive = g_str_equal (sThemeProfile, "adaptive");
        }

        guint nTemperature = 0;
        const gchar *sColorScheme = NULL;
        gchar *sTheme = NULL;
        gint64 nNow = g_get_real_time ();
        gdouble fElevation = solar_elevation((gdouble) nNow / 1000000.0, pImpl->fLatitude, pImpl->fLongitude);

        if (nProfile == 0)
        {
            g_settings_get (pImpl->m_settings, "color-temp", "q", &nTemperature);
        }
        else
        {
            gdouble fShifting = 0.0;

            if (fElevation < SOLAR_CIVIL_TWILIGHT_ELEV)
            {
                fShifting = 1.0;
            }
            else if (fElevation < 3.0)
            {
                fShifting = 1.0 - ((SOLAR_CIVIL_TWILIGHT_ELEV - fElevation) / (SOLAR_CIVIL_TWILIGHT_ELEV - 3.0));
            }

            nTemperature = m_lTempProfiles[nProfile].nTempHigh - (m_lTempProfiles[nProfile].nTempHigh - m_lTempProfiles[nProfile].nTempLow) * fShifting;
            pImpl->bAutoSliderUpdate = TRUE;
        }

        if (!pImpl->bGreeter)
        {
            if (!bThemeAdaptive)
            {
                gchar *sThemeKey = g_strdup_printf ("%s-theme", sThemeProfile);
                sTheme = g_settings_get_string (pImpl->m_settings, sThemeKey);
                g_free (sThemeKey);

                gboolean bLightTheme = g_str_equal (sThemeProfile, "light");

                if (bLightTheme)
                {
                    sColorScheme = "prefer-light";
                }
                else
                {
                    sColorScheme = "prefer-dark";
                }
            }
            else
            {
                if (fElevation < SOLAR_CIVIL_TWILIGHT_ELEV)
                {
                    sColorScheme = "prefer-dark";
                    sTheme = g_settings_get_string (pImpl->m_settings, "dark-theme");
                }
                else
                {
                    sColorScheme = "prefer-light";
                    sTheme = g_settings_get_string (pImpl->m_settings, "light-theme");
                }
            }
        }

        if (pImpl->fLastBrightness != fBrightness || pImpl->nLasColorTemp != nTemperature)
        {
            g_debug ("Calling xsct with %u %f", nTemperature, fBrightness);

            GAction *pAction = g_action_map_lookup_action (G_ACTION_MAP (pImpl->m_action_group), "color-temp");
            GVariant *pTemperature = g_variant_new_double (nTemperature);
            g_action_change_state (pAction, pTemperature);

            GError *pError = NULL;
            gchar *sCommand = g_strdup_printf ("xsct %u %f", nTemperature, fBrightness);
            gboolean bSuccess = g_spawn_command_line_sync (sCommand, NULL, NULL, NULL, &pError);

            if (!bSuccess)
            {
                g_error ("The call to '%s' failed: %s", sCommand, pError->message);
                g_error_free (pError);
            }

            pImpl->fLastBrightness = fBrightness;
            pImpl->nLasColorTemp = nTemperature;
            g_free (sCommand);
            gint nUid = 0;

            if (!pImpl->bGreeter)
            {
                nUid = geteuid ();
            }
            else if (pImpl->sUser)
            {
                const struct passwd *pPasswd = getpwnam (pImpl->sUser);

                if (pPasswd)
                {
                    nUid = pPasswd->pw_uid;
                }
            }

            if (nUid)
            {
                gchar *sPath = g_strdup_printf ("/org/freedesktop/Accounts/User%i", nUid);
                GDBusProxy *pProxy = g_dbus_proxy_new_sync (pImpl->pAccountsServiceConnection, G_DBUS_PROXY_FLAGS_NONE, NULL, "org.freedesktop.Accounts", sPath, "org.freedesktop.DBus.Properties", NULL, NULL);
                g_free (sPath);
                GVariant *pBrightnessValue = g_variant_new ("d", pImpl->fLastBrightness);
                GVariant *pBrightnessParams = g_variant_new ("(ssv)", "org.ayatana.indicator.display.AccountsService", "brightness", pBrightnessValue);
                g_dbus_proxy_call (pProxy, "Set", pBrightnessParams, G_DBUS_CALL_FLAGS_NONE, -1, NULL, NULL, NULL);
                GVariant *pColorTempValue = g_variant_new ("q", pImpl->nLasColorTemp);
                GVariant *pColorTempParams = g_variant_new ("(ssv)", "org.ayatana.indicator.display.AccountsService", "color-temp", pColorTempValue);
                g_dbus_proxy_call (pProxy, "Set", pColorTempParams, G_DBUS_CALL_FLAGS_NONE, -1, NULL, NULL, NULL);
                GVariant *pProfileValue = g_variant_new ("q", nProfile);
                GVariant *pProfileParams = g_variant_new ("(ssv)", "org.ayatana.indicator.display.AccountsService", "color-temp-profile", pProfileValue);
                g_dbus_proxy_call (pProxy, "Set", pProfileParams, G_DBUS_CALL_FLAGS_NONE, -1, NULL, NULL, NULL);
            }
        }

        if (!pImpl->bGreeter)
        {
            gboolean bSameColorScheme = g_str_equal (sColorScheme, pImpl->sLastColorScheme);

            if (pImpl->pColorSchemeSettings && !bSameColorScheme)
            {
                g_debug ("Changing color scheme to %s", sColorScheme);

                g_settings_set_string (pImpl->pColorSchemeSettings, "color-scheme", sColorScheme);
                pImpl->sLastColorScheme = sColorScheme;
            }

            gboolean bSameTheme = FALSE;

            if (pImpl->sLastTheme)
            {
                bSameTheme = g_str_equal (pImpl->sLastTheme, sTheme);
            }

            gboolean bCurrentTheme = g_str_equal ("current", sTheme);

            if (!bSameTheme && !bCurrentTheme)
            {
                g_debug ("Changing theme to %s", sTheme);

                g_settings_set_string (pImpl->pThemeSettings, "gtk-theme", sTheme);
                gchar *sThemePath = g_strdup_printf ("/usr/share/themes/%s/index.theme", sTheme);
                gboolean bThemePath = g_file_test (sThemePath, G_FILE_TEST_EXISTS);

                if (bThemePath)
                {
                    gchar *sFile = NULL;
                    GError *pError = NULL;
                    g_file_get_contents (sThemePath, &sFile, NULL, &pError);

                    if (!pError)
                    {
                        #if GLIB_CHECK_VERSION(2, 73, 0)
                            GRegex *pRegex = g_regex_new ("IconTheme *= *(.*)", G_REGEX_DEFAULT, G_REGEX_MATCH_DEFAULT, &pError);
                        #else
                            GRegex *pRegex = g_regex_new ("IconTheme *= *(.*)", (GRegexCompileFlags) 0, (GRegexMatchFlags) 0, &pError);
                        #endif

                        if (!pError)
                        {
                            GMatchInfo *pMatchInfo = NULL;

                            #if GLIB_CHECK_VERSION(2, 73, 0)
                                gboolean bMatch = g_regex_match (pRegex, sFile, G_REGEX_MATCH_DEFAULT, &pMatchInfo);
                            #else
                                gboolean bMatch = g_regex_match (pRegex, sFile, (GRegexMatchFlags) 0, &pMatchInfo);
                            #endif

                            if (bMatch)
                            {
                                gchar *sIconTheme = g_match_info_fetch (pMatchInfo, 1);
                                g_settings_set_string (pImpl->pThemeSettings, "icon-theme", sIconTheme);
                                g_free (sIconTheme);
                            }
                            else
                            {
                                g_warning ("/usr/share/themes/%s/index.theme does not define an IconTheme", sTheme);
                            }

                            g_match_info_free (pMatchInfo);
                            g_regex_unref (pRegex);
                        }
                        else
                        {
                            g_error ("PANIC: Failed to compile regex: %s", pError->message);
                            g_error_free (pError);
                        }

                        #if GLIB_CHECK_VERSION(2, 73, 0)
                            pRegex = g_regex_new ("MetacityTheme *= *(.*)", G_REGEX_DEFAULT, G_REGEX_MATCH_DEFAULT, &pError);
                        #else
                            pRegex = g_regex_new ("MetacityTheme *= *(.*)", (GRegexCompileFlags) 0, (GRegexMatchFlags) 0, &pError);
                        #endif

                        if (!pError)
                        {
                            GMatchInfo *pMatchInfo = NULL;

                            #if GLIB_CHECK_VERSION(2, 73, 0)
                                gboolean bMatch = g_regex_match (pRegex, sFile, G_REGEX_MATCH_DEFAULT, &pMatchInfo);
                            #else
                                gboolean bMatch = g_regex_match (pRegex, sFile, (GRegexMatchFlags) 0, &pMatchInfo);
                            #endif

                            if (bMatch)
                            {
                                gchar *sMetacityTheme = g_match_info_fetch (pMatchInfo, 1);
                                g_settings_set_string (pImpl->pMetacitySettings, "theme", sMetacityTheme);
                                g_free (sMetacityTheme);
                            }
                            else
                            {
                                g_warning ("/usr/share/themes/%s/index.theme does not define a MetacityTheme", sTheme);
                            }

                            g_match_info_free (pMatchInfo);
                            g_regex_unref (pRegex);
                        }
                        else
                        {
                            g_error ("PANIC: Failed to compile regex: %s", pError->message);
                            g_error_free (pError);
                        }

                        #if GLIB_CHECK_VERSION(2, 73, 0)
                            pRegex = g_regex_new ("CursorTheme *= *(.*)", G_REGEX_DEFAULT, G_REGEX_MATCH_DEFAULT, &pError);
                        #else
                            pRegex = g_regex_new ("CursorTheme *= *(.*)", (GRegexCompileFlags) 0, (GRegexMatchFlags) 0, &pError);
                        #endif

                        if (!pError)
                        {
                            GMatchInfo *pMatchInfo = NULL;

                            #if GLIB_CHECK_VERSION(2, 73, 0)
                                gboolean bMatch = g_regex_match (pRegex, sFile, G_REGEX_MATCH_DEFAULT, &pMatchInfo);
                            #else
                                gboolean bMatch = g_regex_match (pRegex, sFile, (GRegexMatchFlags) 0, &pMatchInfo);
                            #endif

                            if (bMatch)
                            {
                                gchar *sCursorTheme = g_match_info_fetch (pMatchInfo, 1);
                                g_settings_set_string (pImpl->pCursorSettings, "cursor-theme", sTheme);
                                g_free (sCursorTheme);
                            }
                            else
                            {
                                g_warning ("/usr/share/themes/%s/index.theme does not define a CursorTheme", sTheme);
                            }

                            g_match_info_free (pMatchInfo);
                            g_regex_unref (pRegex);
                        }
                        else
                        {
                            g_error ("PANIC: Failed to compile regex: %s", pError->message);
                            g_error_free (pError);
                        }

                        g_free (sFile);
                    }
                    else
                    {
                        g_error ("PANIC: Failed to get index.theme contents: %s", pError->message);
                        g_error_free (pError);
                    }
                }
                else
                {
                    g_warning ("/usr/share/themes/%s/index.theme does not exist", sTheme);
                }

                g_free (sThemePath);

                if (pImpl->sLastTheme)
                {
                    g_free (pImpl->sLastTheme);
                }

                pImpl->sLastTheme = g_strdup (sTheme);
            }

            g_free (sTheme);
            g_free (sThemeProfile);
        }

        return G_SOURCE_CONTINUE;
    }

    static void onGeoClueLoaded (GObject *pObject, GAsyncResult *pResult, gpointer pData)
    {
        DisplayIndicator::Impl *pImpl = (DisplayIndicator::Impl*) pData;
        GError *pError = NULL;
        GClueSimple *pSimple = gclue_simple_new_finish (pResult, &pError);

        if (pError != NULL)
        {
            g_warning ("Failed to connect to GeoClue2 service: %s", pError->message);
        }
        else
        {
            GClueLocation *pLocation = gclue_simple_get_location (pSimple);
            pImpl->fLatitude = gclue_location_get_latitude (pLocation);
            pImpl->fLongitude = gclue_location_get_longitude (pLocation);
            g_settings_set_double (pImpl->m_settings, "latitude", pImpl->fLatitude);
            g_settings_set_double (pImpl->m_settings, "longitude", pImpl->fLongitude);
        }

        updateColor (pImpl);
    }

    static void onColorTempSettings (GSettings *pSettings, const gchar *sKey, gpointer pData)
    {
        GVariant *pProfile = g_variant_new_uint16 (0);
        g_settings_set_value (pSettings, "color-temp-profile", pProfile);

        updateColor (pData);
    }

    static gboolean settingsIntToActionStateString (GValue *pValue, GVariant *pVariant, gpointer pData)
    {
        guint16 nVariant = g_variant_get_uint16 (pVariant);
        gchar *sVariant = g_strdup_printf ("%u", nVariant);
        GVariant *pVariantString = g_variant_new_string (sVariant);
        g_free (sVariant);
        g_value_set_variant (pValue, pVariantString);

        return TRUE;
    }

    static GVariant* actionStateStringToSettingsInt (const GValue *pValue, const GVariantType *pVariantType, gpointer pData)
    {
        GVariant *pVariantString = g_value_get_variant (pValue);
        const gchar *sValue = g_variant_get_string (pVariantString, NULL);
        guint16 nValue = (guint16) g_ascii_strtoull (sValue, NULL, 10);
        GVariant *pVariantInt = g_variant_new_uint16 (nValue);
        GValue cValue = G_VALUE_INIT;
        g_value_init (&cValue, G_TYPE_VARIANT);
        g_value_set_variant (&cValue, pVariantInt);

        return g_value_dup_variant (&cValue);
    }

    static void onColorTempState (GSimpleAction *pAction, GVariant *pVariant, gpointer pData)
    {
        g_simple_action_set_state (pAction, pVariant);

        DisplayIndicator::Impl *pImpl = (DisplayIndicator::Impl*) pData;

        if (pImpl->bAutoSliderUpdate)
        {
            pImpl->bAutoSliderUpdate = FALSE;

            return;
        }

        GVariant *pProfile = g_variant_new_uint16 (0);
        g_settings_set_value (pImpl->m_settings, "color-temp-profile", pProfile);

        guint16 nTemperature = (guint16) g_variant_get_double (pVariant);
        GVariant *pTemperature = g_variant_new_uint16 (nTemperature);
        g_settings_set_value (pImpl->m_settings, "color-temp", pTemperature);
    }
#endif

  /***
  ****  Actions
  ***/

  static gboolean settings_to_action_state(GValue *value,
                                           GVariant *variant,
                                           gpointer /*unused*/)
  {
    g_value_set_variant(value, variant);
    return TRUE;
  }

  static GVariant* action_state_to_settings(const GValue *value,
                                            const GVariantType * /*expected_type*/,
                                            gpointer /*unused*/)
  {
    return g_value_dup_variant(value);
  }

  GSimpleActionGroup* create_action_group()
  {
    GSimpleActionGroup* group;
    GSimpleAction* action;

    group = g_simple_action_group_new();
    GVariantType *pVariantType = g_variant_type_new("b");
    action = g_simple_action_new_stateful("rotation-lock",
                                          pVariantType,
                                          g_variant_new_boolean(false));
    g_variant_type_free(pVariantType);
    g_settings_bind_with_mapping(m_settings, "rotation-lock",
                                 action, "state",
                                 G_SETTINGS_BIND_DEFAULT,
                                 settings_to_action_state,
                                 action_state_to_settings,
                                 nullptr,
                                 nullptr);

    g_action_map_add_action(G_ACTION_MAP(group), G_ACTION(action));
    g_object_unref(G_OBJECT(action));
    g_signal_connect_swapped(m_settings, "changed::rotation-lock",
                             G_CALLBACK(on_rotation_lock_setting_changed), this);

#ifdef COLOR_TEMP_ENABLED
    if (ayatana_common_utils_is_lomiri() == FALSE)
    {
        pVariantType = g_variant_type_new ("d");
        guint nTemperature = 0;
        g_settings_get (this->m_settings, "color-temp", "q", &nTemperature);
        action = g_simple_action_new_stateful ("color-temp", pVariantType, g_variant_new_double (nTemperature));
        g_variant_type_free (pVariantType);
        g_action_map_add_action (G_ACTION_MAP (group), G_ACTION (action));
        g_signal_connect (m_settings, "changed::color-temp", G_CALLBACK (onColorTempSettings), this);
        g_signal_connect (action, "change-state", G_CALLBACK (onColorTempState), this);
        g_object_unref(G_OBJECT (action));

        pVariantType = g_variant_type_new ("s");
        guint nProfile = 0;
        g_settings_get (this->m_settings, "color-temp-profile", "q", &nProfile);
        gchar *sProfile = g_strdup_printf ("%i", nProfile);
        action = g_simple_action_new_stateful ("profile", pVariantType, g_variant_new_string (sProfile));
        g_free (sProfile);
        g_variant_type_free (pVariantType);
        g_settings_bind_with_mapping (this->m_settings, "color-temp-profile", action, "state", G_SETTINGS_BIND_DEFAULT, settingsIntToActionStateString, actionStateStringToSettingsInt, NULL, NULL);
        g_action_map_add_action(G_ACTION_MAP(group), G_ACTION(action));
        g_object_unref(G_OBJECT(action));
        g_signal_connect_swapped (m_settings, "changed::color-temp-profile", G_CALLBACK (updateColor), this);

        pVariantType = g_variant_type_new("d");
        gdouble fBrightness = g_settings_get_double (this->m_settings, "brightness");
        action = g_simple_action_new_stateful ("brightness", pVariantType, g_variant_new_double (fBrightness));
        g_variant_type_free(pVariantType);
        g_settings_bind_with_mapping (m_settings, "brightness", action, "state", G_SETTINGS_BIND_DEFAULT, settings_to_action_state, action_state_to_settings, NULL, NULL);
        g_action_map_add_action (G_ACTION_MAP (group), G_ACTION (action));
        g_object_unref (G_OBJECT (action));
        g_signal_connect_swapped (m_settings, "changed::brightness", G_CALLBACK (updateColor), this);

        if (!this->bGreeter)
        {
            pVariantType = g_variant_type_new ("s");
            action = g_simple_action_new_stateful ("theme", pVariantType, g_variant_new_string ("light"));
            g_variant_type_free (pVariantType);
            g_settings_bind_with_mapping (this->m_settings, "theme-profile", action, "state", G_SETTINGS_BIND_DEFAULT, settings_to_action_state, action_state_to_settings, NULL, NULL);
            g_action_map_add_action(G_ACTION_MAP(group), G_ACTION(action));
            g_object_unref(G_OBJECT(action));
            g_signal_connect_swapped (m_settings, "changed::theme-profile", G_CALLBACK (updateColor), this);
            g_signal_connect_swapped (m_settings, "changed::light-theme", G_CALLBACK (updateColor), this);
            g_signal_connect_swapped (m_settings, "changed::dark-theme", G_CALLBACK (updateColor), this);
        }
    }
#endif

    if (!this->bGreeter)
    {
        action = g_simple_action_new ("settings", NULL);
        g_action_map_add_action (G_ACTION_MAP (group), G_ACTION (action));
        g_signal_connect (action, "activate", G_CALLBACK (onSettings), this);
        g_object_unref (G_OBJECT (action));
    }

    return group;
  }

  /***
  ****  Phone profile
  ***/

  static void on_rotation_lock_setting_changed (gpointer gself)
  {
    static_cast<Impl*>(gself)->update_phone_header();
  }

  GMenuModel* create_phone_menu()
  {
    GMenu* menu;
    GMenu* section;
    GMenuItem* menu_item;

    menu = g_menu_new();
    section = g_menu_new();
    menu_item = g_menu_item_new(_("Rotation Lock"), "indicator.rotation-lock(true)");
    g_menu_item_set_attribute(menu_item, "x-ayatana-type", "s", "org.ayatana.indicator.switch");
    g_menu_append_item(section, menu_item);
    g_menu_append_section(menu, NULL, G_MENU_MODEL(section));
    g_object_unref(section);
    g_object_unref(menu_item);

    return G_MENU_MODEL(menu);
  }

  static void onSettings (GSimpleAction *pAction, GVariant *pVariant, gpointer pData)
  {
    if (ayatana_common_utils_is_mate ())
    {
      ayatana_common_utils_execute_command ("mate-display-properties");
    }
    else if (ayatana_common_utils_is_xfce ())
    {
      ayatana_common_utils_execute_command ("xfce4-display-settings");
    }
    else
    {
      ayatana_common_utils_execute_command ("gnome-control-center display");
    }
  }

  GMenuModel* create_desktop_menu()
  {
    GMenu* menu;
    GMenu* section;
    GMenuItem* menu_item;

    menu = g_menu_new();

    if (ayatana_common_utils_is_lomiri())
    {
        section = g_menu_new();
        menu_item = g_menu_item_new(_("Rotation Lock"), "indicator.rotation-lock(true)");
        g_menu_item_set_attribute(menu_item, "x-ayatana-type", "s", "org.ayatana.indicator.switch");
        g_menu_append_item(section, menu_item);
        g_menu_append_section(menu, NULL, G_MENU_MODEL(section));
        g_object_unref(section);
        g_object_unref(menu_item);
    }
    else
    {
#ifdef COLOR_TEMP_ENABLED
        section = g_menu_new ();

        GIcon *pIconMin = g_themed_icon_new_with_default_fallbacks ("ayatana-indicator-display-brightness-low");
        GIcon *pIconMax = g_themed_icon_new_with_default_fallbacks ("ayatana-indicator-display-brightness-high");
        GVariant *pIconMinSerialised = g_icon_serialize (pIconMin);
        GVariant *pIconMaxSerialised = g_icon_serialize (pIconMax);
        menu_item = g_menu_item_new (_("Brightness"), "indicator.brightness");
        g_menu_item_set_attribute (menu_item, "x-ayatana-type", "s", "org.ayatana.indicator.slider");
        g_menu_item_set_attribute (menu_item, "x-ayatana-type", "s", "org.ayatana.indicator.slider");
        g_menu_item_set_attribute_value (menu_item, "min-icon", pIconMinSerialised);
        g_menu_item_set_attribute_value (menu_item, "max-icon", pIconMaxSerialised);
        g_menu_item_set_attribute (menu_item, "min-value", "d", 0.5);
        g_menu_item_set_attribute (menu_item, "max-value", "d", 1.0);
        g_menu_item_set_attribute (menu_item, "step", "d", 0.01);
        g_menu_item_set_attribute (menu_item, "digits", "y", 2);
        g_menu_append_item (section, menu_item);

        pIconMin = g_themed_icon_new_with_default_fallbacks ("ayatana-indicator-display-colortemp-on");
        pIconMax = g_themed_icon_new_with_default_fallbacks ("ayatana-indicator-display-colortemp-off");
        pIconMinSerialised = g_icon_serialize (pIconMin);
        pIconMaxSerialised = g_icon_serialize (pIconMax);
        menu_item = g_menu_item_new (_("Color temperature"), "indicator.color-temp");
        g_menu_item_set_attribute (menu_item, "x-ayatana-type", "s", "org.ayatana.indicator.slider");
        g_menu_item_set_attribute (menu_item, "x-ayatana-type", "s", "org.ayatana.indicator.slider");
        g_menu_item_set_attribute_value (menu_item, "min-icon", pIconMinSerialised);
        g_menu_item_set_attribute_value (menu_item, "max-icon", pIconMaxSerialised);
        g_menu_item_set_attribute (menu_item, "min-value", "d", 3000.0);
        g_menu_item_set_attribute (menu_item, "max-value", "d", 6500.0);
        g_menu_item_set_attribute (menu_item, "step", "d", 100.0);
        g_menu_item_set_attribute (menu_item, "digits", "y", 0);
        g_menu_append_item (section, menu_item);

        GMenu *pMenuProfiles = g_menu_new ();
        GMenuItem *pItemProfiles = g_menu_item_new_submenu (_("Color temperature profile"), G_MENU_MODEL (pMenuProfiles));
        guint nProfile = 0;

        while (m_lTempProfiles[nProfile].sName != NULL)
        {
            gchar *sAction = g_strdup_printf ("indicator.profile::%u", nProfile);
            gchar *sName = gettext (m_lTempProfiles[nProfile].sName);
            GMenuItem *pItemProfile = g_menu_item_new (sName, sAction);
            g_free(sAction);
            g_menu_append_item (pMenuProfiles, pItemProfile);
            g_object_unref (pItemProfile);

            nProfile++;
        }

        g_menu_append_item (section, pItemProfiles);
        g_object_unref (pItemProfiles);
        g_object_unref (pMenuProfiles);

        g_menu_append_section (menu, NULL, G_MENU_MODEL (section));
        g_object_unref (pIconMin);
        g_object_unref (pIconMax);
        g_variant_unref (pIconMinSerialised);
        g_variant_unref (pIconMaxSerialised);
        g_object_unref (section);
        g_object_unref (menu_item);

        if (!this->bGreeter)
        {
            section = g_menu_new ();
            pMenuProfiles = g_menu_new ();
            pItemProfiles = g_menu_item_new_submenu (_("Theme profile"), G_MENU_MODEL (pMenuProfiles));
            GMenuItem *pItemProfile = g_menu_item_new (_("Light"), "indicator.theme::light");
            g_menu_append_item (pMenuProfiles, pItemProfile);
            g_object_unref (pItemProfile);
            pItemProfile = g_menu_item_new (_("Dark"), "indicator.theme::dark");
            g_menu_append_item (pMenuProfiles, pItemProfile);
            g_object_unref (pItemProfile);
            pItemProfile = g_menu_item_new (_("Adaptive"), "indicator.theme::adaptive");
            g_menu_append_item (pMenuProfiles, pItemProfile);
            g_object_unref (pItemProfile);
            g_menu_append_item (section, pItemProfiles);
            g_object_unref (pItemProfiles);
            g_object_unref (pMenuProfiles);
            g_menu_append_section (menu, NULL, G_MENU_MODEL (section));
            g_object_unref (section);
        }
#endif
        // cppcheck fails on !this->bGreeter
        if (this->bGreeter == FALSE)
        {
            section = g_menu_new ();
            menu_item = g_menu_item_new (_("Display settings…"), "indicator.settings");
            g_menu_append_item (section, menu_item);
            g_menu_append_section (menu, NULL, G_MENU_MODEL (section));
            g_object_unref (section);
            g_object_unref (menu_item);
        }
    }

    return G_MENU_MODEL(menu);
  }

  void update_phone_header()
  {
    Header h;
    h.title = _("Rotation");
    h.tooltip = h.title;
    h.a11y = h.title;
    h.is_visible = g_settings_get_boolean(m_settings, "rotation-lock");
    h.icon = m_icon;
    m_phone->header().set(h);
  }

  void update_desktop_header()
  {
    Header h;
    h.title = _("Display");
    h.tooltip = _("Display settings and features");
    h.a11y = h.title;
    h.is_visible = TRUE;
    h.icon = m_icon;
    m_desktop->header().set(h);
  }

  /***
  ****
  ***/

  GSettings* m_settings = nullptr;
  GSimpleActionGroup* m_action_group = nullptr;
  std::shared_ptr<SimpleProfile> m_phone;
  std::shared_ptr<SimpleProfile> m_desktop;
  std::shared_ptr<GIcon> m_icon;
  gboolean bGreeter;
#ifdef COLOR_TEMP_ENABLED
  gdouble fLatitude = 0.0;
  gdouble fLongitude = 0.0;
  gboolean bAutoSliderUpdate = FALSE;
  guint nCallback = 0;
  gdouble fLastBrightness = 0.0;
  guint nLasColorTemp = 0;
  gchar *sLastTheme = NULL;
  const gchar *sLastColorScheme = "default";
  GSettings *pThemeSettings = NULL;
  GSettings *pCursorSettings = NULL;
  GSettings *pMetacitySettings = NULL;
  GSettings *pColorSchemeSettings = NULL;
  gboolean bTest;
  guint nGreeterSubscription = 0;
  GDBusConnection *pConnection = NULL;
  gchar *sUser = NULL;
  GSList *lUsers = NULL;
  gboolean bReadingAccountsService = FALSE;
  GDBusConnection *pAccountsServiceConnection = NULL;
#endif
};

/***
****
***/

DisplayIndicator::DisplayIndicator():
    impl(new Impl())
{
}

DisplayIndicator::~DisplayIndicator()
{
}

std::vector<std::shared_ptr<Profile>>
DisplayIndicator::profiles() const
{
  return impl->profiles();
}

GSimpleActionGroup*
DisplayIndicator::action_group() const
{
  return impl->action_group();
}

const char*
DisplayIndicator::name() const
{
  return "display";
}

/***
****
***/

