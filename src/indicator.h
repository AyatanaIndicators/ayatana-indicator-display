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

#ifndef INDICATOR_DISPLAY_INDICATOR_H
#define INDICATOR_DISPLAY_INDICATOR_H

#include <core/property.h>

#include <gio/gio.h> // GIcon

#include <string>
#include <vector>

struct Header
{
  bool is_visible = false;
  std::string title;
  std::string label;
  std::string a11y;
  std::shared_ptr<GIcon> icon;

  bool operator== (const Header& that) const {
    return (is_visible == that.is_visible) &&
           (title == that.title) &&
           (label == that.label) &&
           (a11y == that.a11y) &&
           (icon == that.icon);
  }
  bool operator!= (const Header& that) const { return !(*this == that);}
};


class Profile
{
public:
  virtual std::string name() const =0;
  virtual const core::Property<Header>& header() const =0;
  virtual std::shared_ptr<GMenuModel> menu_model() const =0;

protected:
  Profile() =default;
};


class SimpleProfile: public Profile
{
public:
  SimpleProfile(const char* name, const std::shared_ptr<GMenuModel>& menu): m_name(name), m_menu(menu) {}

  std::string name() const {return m_name;}
  core::Property<Header>& header() {return m_header;}
  const core::Property<Header>& header() const {return m_header;}
  std::shared_ptr<GMenuModel> menu_model() const {return m_menu;}

protected:
  const std::string m_name;
  core::Property<Header> m_header;
  std::shared_ptr<GMenuModel> m_menu;
};


class Indicator
{
public:
  virtual ~Indicator() =default;

  virtual const char* name() const =0;
  virtual GSimpleActionGroup* action_group() const =0;
  virtual std::vector<std::shared_ptr<Profile>> profiles() const =0;
};

#endif
