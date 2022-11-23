/*
 * Copyright 2014 Canonical Ltd.
 * Copyright 2022 Robert Tari
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

#include <src/rotation-lock.h>
#include <glib-unix.h>
#include <glib/gi18n.h>

extern "C"
{
    #include <ayatana/common/utils.h>
}

class RotationLockIndicator::Impl
{
public:

  Impl()
  {
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

        }
    }

    m_action_group = create_action_group();

    // build the icon
    const char *rotation_lock_icon_name {"orientation-lock"};

    if (!ayatana_common_utils_is_lomiri())
    {
        rotation_lock_icon_name = "display-panel";
    }

    auto icon = g_themed_icon_new_with_default_fallbacks(rotation_lock_icon_name);
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

    g_unix_signal_add (SIGINT, onSigInt, m_settings);
    onColorTemp (m_settings, "color-temp", NULL);
  }

  ~Impl()
  {
    onColorTemp (m_settings, "color-temp", GUINT_TO_POINTER (6500));
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

  static gboolean onSigInt (gpointer pData)
  {
    onColorTemp (G_SETTINGS (pData), "color-temp", GUINT_TO_POINTER (6500));

    return G_SOURCE_REMOVE;
  }

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

  static gboolean settingsToActionStateDouble (GValue *pValue, GVariant *pVariant, gpointer pData)
  {
    gdouble fVariant = (gdouble) g_variant_get_uint16 (pVariant);
    GVariant *pVariantDouble = g_variant_new_double (fVariant);
    g_value_set_variant (pValue, pVariantDouble);

    return TRUE;
  }

  static GVariant* actionStateToSettingsInt (const GValue *pValue, const GVariantType *pVariantType, gpointer pData)
  {
    GVariant *pVariantDouble = g_value_get_variant (pValue);
    guint16 nValue = (guint16) g_variant_get_double (pVariantDouble);
    GVariant *pVariantInt = g_variant_new_uint16 (nValue);
    GValue cValue = G_VALUE_INIT;
    g_value_init (&cValue, G_TYPE_VARIANT);
    g_value_set_variant (&cValue, pVariantInt);

    return g_value_dup_variant (&cValue);
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

    pVariantType = g_variant_type_new ("d");
    action = g_simple_action_new_stateful ("color-temp", pVariantType, g_variant_new_double (0));
    g_variant_type_free (pVariantType);
    g_settings_bind_with_mapping (m_settings, "color-temp", action, "state", G_SETTINGS_BIND_DEFAULT, settingsToActionStateDouble, actionStateToSettingsInt, NULL, NULL);
    g_action_map_add_action (G_ACTION_MAP (group), G_ACTION (action));
    g_object_unref(G_OBJECT (action));
    g_signal_connect (m_settings, "changed::color-temp", G_CALLBACK (onColorTemp), NULL);

    pVariantType = g_variant_type_new ("s");
    action = g_simple_action_new_stateful ("profile", pVariantType, g_variant_new_string("1"));
    g_variant_type_free (pVariantType);
    g_action_map_add_action(G_ACTION_MAP(group), G_ACTION(action));
    g_object_unref(G_OBJECT(action));

    action = g_simple_action_new ("settings", NULL);
    g_action_map_add_action (G_ACTION_MAP (group), G_ACTION (action));
    g_object_unref (G_OBJECT (action));
    g_signal_connect (action, "activate", G_CALLBACK (onSettings), this);

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
    menu_item = g_menu_item_new(_("Rotation Lock"), "indicator.rotation-lock");
    g_menu_item_set_attribute(menu_item, "x-ayatana-type", "s", "org.ayatana.indicator.switch");
    g_menu_append_item(section, menu_item);
    g_menu_append_section(menu, NULL, G_MENU_MODEL(section));
    g_object_unref(section);
    g_object_unref(menu_item);

    return G_MENU_MODEL(menu);
  }

  static void onColorTemp (GSettings *pSettings, const gchar *sKey, gpointer pData)
  {
    guint16 nTemp = 0;

    if (pData)
    {
      nTemp = GPOINTER_TO_UINT (pData);
    }
    else
    {
      GVariant *pTemp = g_settings_get_value (pSettings, sKey);
      nTemp = g_variant_get_uint16 (pTemp);
    }

    GError *pError = NULL;
    gchar *sCommand = g_strdup_printf ("xsct %u", nTemp);
    gboolean bSuccess = g_spawn_command_line_sync (sCommand, NULL, NULL, NULL, &pError);

    if (!bSuccess)
    {
      g_error ("The call to '%s' failed: %s", sCommand, pError->message);
      g_error_free (pError);
    }

    g_free (sCommand);
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
        menu_item = g_menu_item_new(_("Rotation Lock"), "indicator.rotation-lock");
        g_menu_item_set_attribute(menu_item, "x-ayatana-type", "s", "org.ayatana.indicator.switch");
        g_menu_append_item(section, menu_item);
        g_menu_append_section(menu, NULL, G_MENU_MODEL(section));
        g_object_unref(section);
        g_object_unref(menu_item);
    }
    else
    {
        section = g_menu_new ();
        GIcon *pIconMin = g_themed_icon_new_with_default_fallbacks ("ayatana-indicator-display-colortemp-on");
        GIcon *pIconMax = g_themed_icon_new_with_default_fallbacks ("ayatana-indicator-display-colortemp-off");
        GVariant *pIconMinSerialised = g_icon_serialize (pIconMin);
        GVariant *pIconMaxSerialised = g_icon_serialize (pIconMax);
        menu_item = g_menu_item_new (_("Color temperature"), "indicator.color-temp");
        g_menu_item_set_attribute (menu_item, "x-ayatana-type", "s", "org.ayatana.indicator.slider");
        g_menu_item_set_attribute (menu_item, "x-ayatana-type", "s", "org.ayatana.indicator.slider");
        g_menu_item_set_attribute_value (menu_item, "min-icon", pIconMinSerialised);
        g_menu_item_set_attribute_value (menu_item, "max-icon", pIconMaxSerialised);
        g_menu_item_set_attribute (menu_item, "min-value", "d", 3500.0);
        g_menu_item_set_attribute (menu_item, "max-value", "d", 6500.0);
        g_menu_item_set_attribute (menu_item, "step", "d", 100.0);
        g_menu_append_item (section, menu_item);

        GMenu *pMenuProfiles = g_menu_new ();
        GMenuItem *pItemProfile1 = g_menu_item_new (_("Manual"), "indicator.profile::1");
        GMenuItem *pItemProfiles = g_menu_item_new_submenu (_("Color temperature profiles"), G_MENU_MODEL (pMenuProfiles));
        g_menu_append_item (pMenuProfiles, pItemProfile1);
        g_object_unref (pItemProfile1);
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

        section = g_menu_new ();
        menu_item = g_menu_item_new (_("Display settings…"), "indicator.settings");
        g_menu_append_item (section, menu_item);
        g_menu_append_section (menu, NULL, G_MENU_MODEL (section));
        g_object_unref (section);
        g_object_unref (menu_item);
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
};

/***
****
***/

RotationLockIndicator::RotationLockIndicator():
    impl(new Impl())
{
}

RotationLockIndicator::~RotationLockIndicator()
{
}

std::vector<std::shared_ptr<Profile>>
RotationLockIndicator::profiles() const
{
  return impl->profiles();
}

GSimpleActionGroup*
RotationLockIndicator::action_group() const
{
  return impl->action_group();
}

const char*
RotationLockIndicator::name() const
{
  return "rotation_lock";
}

/***
****
***/

