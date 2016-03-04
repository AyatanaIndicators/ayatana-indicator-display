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

#include "test-dbus-fixture.h"

#include <src/rotation-lock.h>

class RotationLockFixture: public TestDBusFixture
{
private:
  typedef TestDBusFixture super;

protected:

  void SetUp()
  {
    super::SetUp();
  }

  void TearDown()
  {
    super::TearDown();
  }
};

/***
****
***/

TEST_F(RotationLockFixture, CheckIndicator)
{
  RotationLockIndicator indicator;

  ASSERT_STREQ("rotation_lock", indicator.name());
  auto actions = indicator.action_group();
  ASSERT_TRUE(actions != nullptr);
  ASSERT_TRUE(g_action_group_has_action(G_ACTION_GROUP(actions), "rotation-lock"));

  std::vector<std::shared_ptr<Profile>> profiles = indicator.profiles();
  ASSERT_EQ(1, profiles.size());
  std::shared_ptr<Profile> phone = profiles[0];
  ASSERT_EQ(std::string("phone"), phone->name());
  ASSERT_FALSE(phone->header()->is_visible);
}

