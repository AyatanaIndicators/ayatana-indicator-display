/*
 * Copyright 2014 Canonical Ltd.
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 3, as published
 * by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranties of
 * MERCHANTABILITY, SATISFACTORY QUALITY, or FITNESS FOR A PARTICULAR
 * PURPOSE.  See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors:
 *   Charles Kerr <charles.kerr@canonical.com>
 */

#include <src/exporter.h>

class Exporter::Impl
{
public:

  Impl(const std::shared_ptr<Indicator>& indicator):
    m_indicator(indicator)
  {
    auto bus_name = g_strdup_printf("com.canonical.indicator.%s", indicator->name());
    m_own_id = g_bus_own_name(G_BUS_TYPE_SESSION,
                              bus_name,
                              G_BUS_NAME_OWNER_FLAGS_NONE,
                              on_bus_acquired,
                              nullptr,
                              on_name_lost,
                              this,
                              nullptr);

    g_free(bus_name);
  }

  ~Impl()
  {
    if (m_bus != nullptr)
    {
      for(const auto& id : m_exported_menu_ids)
        g_dbus_connection_unexport_menu_model(m_bus, id);

      if (m_exported_actions_id)
        g_dbus_connection_unexport_action_group(m_bus, m_exported_actions_id);
    }

    if (m_own_id)
      g_bus_unown_name(m_own_id);

    g_clear_object(&m_bus);
  }

  core::Signal<std::string>& name_lost()
  {
    return m_name_lost;
  }

private:

  void emit_name_lost(const char* bus_name)
  {
    m_name_lost(bus_name);
  }

  static void on_bus_acquired(GDBusConnection * connection,
                              const gchar     * name,
                              gpointer          gself)
  {
    static_cast<Impl*>(gself)->on_bus_acquired(connection, name);
  }

  void on_bus_acquired(GDBusConnection* connection, const gchar* /*name*/)
  {
    m_bus = G_DBUS_CONNECTION(g_object_ref(G_OBJECT(connection)));

    export_actions(m_indicator);

    for (auto& profile : m_indicator->profiles())
      export_profile(m_indicator, profile);
  }

  void export_actions(const std::shared_ptr<Indicator>& indicator)
  {
    GError* error;
    char* object_path;
    guint id;
     
    // export the actions

    error = nullptr;
    object_path = g_strdup_printf("/com/canonical/indicator/%s", indicator->name());
    id = g_dbus_connection_export_action_group(m_bus,
                                               object_path,
                                               G_ACTION_GROUP(indicator->action_group()),
                                               &error);
    if (id)
      m_exported_actions_id = id;
    else
      g_warning("Can't export action group to '%s': %s", object_path, error->message);

    g_clear_error(&error);
    g_free(object_path);
  }

  static GVariant* create_header_state(const Header& h)
  {
    GVariantBuilder b;
    g_variant_builder_init(&b, G_VARIANT_TYPE_VARDICT);

    g_variant_builder_add(&b, "{sv}", "visible", g_variant_new_boolean(h.is_visible));

    if (!h.title.empty())
      g_variant_builder_add(&b, "{sv}", "title", g_variant_new_string(h.title.c_str()));

    if (!h.label.empty())
      g_variant_builder_add(&b, "{sv}", "label", g_variant_new_string(h.label.c_str()));

    if (!h.title.empty() || !h.label.empty())
      g_variant_builder_add(&b, "{sv}", "accessible-desc", g_variant_new_string(!h.label.empty() ? h.label.c_str() : h.title.c_str()));

    if (h.icon)
      g_variant_builder_add(&b, "{sv}", "icon", g_icon_serialize(h.icon.get()));

    return g_variant_builder_end (&b);
  }

  void export_profile(const std::shared_ptr<Indicator>& indicator,
                      const std::shared_ptr<Profile>& profile)
  {
    // build the header action
    auto action_group = indicator->action_group();
    std::string action_name = profile->name() + "-header";
    auto a = g_simple_action_new_stateful(action_name.c_str(), nullptr, create_header_state(profile->header()));
    g_action_map_add_action(G_ACTION_MAP(action_group), G_ACTION(a));
    profile->header().changed().connect([action_group,action_name](const Header& header){
        auto state = create_header_state(header);
        auto tmp = g_variant_print(state, true);
        g_debug("header changed; updating action state to '%s'", tmp);
        g_action_group_change_action_state(G_ACTION_GROUP(action_group), 
                                           action_name.c_str(),
                                           create_header_state(header));
        g_free(tmp);
    });

    // build the header menu
    auto detailed_action = g_strdup_printf("indicator.%s", action_name.c_str());
    GMenuItem* header = g_menu_item_new(nullptr, detailed_action);
    g_menu_item_set_attribute(header, "x-canonical-type", "s", "com.canonical.indicator.root");
    g_menu_item_set_submenu(header, profile->menu_model().get());
    g_free(detailed_action);

    // build the menu
    auto menu = g_menu_new();
    g_menu_append_item(menu, header);
    g_object_unref(header);

    // export the menu
    auto object_path = g_strdup_printf("/com/canonical/indicator/%s/%s",
                                       indicator->name(),
                                       profile->name().c_str());
    GError* error = nullptr;
    auto id = g_dbus_connection_export_menu_model(m_bus, object_path, G_MENU_MODEL(menu), &error);
    if (id)
      m_exported_menu_ids.insert(id);
    else if (error != nullptr)
      g_warning("cannot export '%s': %s", object_path, error->message);

    g_free(object_path);
    g_clear_error(&error);
    //g_object_unref(menu);
  }

  static void on_name_lost(GDBusConnection * /*connection*/,
                           const gchar     * name,
                           gpointer          gthis)
  {
    static_cast<Impl*>(gthis)->emit_name_lost(name);
  }

  const std::string m_bus_name;
  core::Signal<std::string> m_name_lost;
  std::shared_ptr<Indicator> m_indicator;
  std::set<guint> m_exported_menu_ids;
  guint m_own_id = 0;
  guint m_exported_actions_id = 0;
  GDBusConnection * m_bus = nullptr;
};

/***
****
***/

Exporter::Exporter(const std::shared_ptr<Indicator>& indicator):
  impl(new Impl(indicator))
{
}

Exporter::~Exporter()
{
}

core::Signal<std::string>&
Exporter::name_lost()
{
  return impl->name_lost();
}

/***
****
***/

