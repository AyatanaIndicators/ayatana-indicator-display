'''unity greeter mock template

Very basic template that just mocks the greeter is-active flag
'''

# This program is free software; you can redistribute it and/or modify it under
# the terms of the GNU Lesser General Public License as published by the Free
# Software Foundation; either version 3 of the License, or (at your option) any
# later version.  See http://www.gnu.org/copyleft/lgpl.html for the full text
# of the license.

__author__ = 'Charles Kerr'
__email__ = 'charles.kerr@canonical.com'
__copyright__ = '(c) 2016 Canonical Ltd.'
__license__ = 'LGPL 3+'

import dbus
import os

from dbusmock import MOCK_IFACE, mockobject

BUS_NAME = 'com.canonical.UnityGreeter'
MAIN_OBJ = '/'
MAIN_IFACE = 'com.canonical.UnityGreeter'
SYSTEM_BUS = False


def load(mock, parameters):
    mock.AddMethods(
        MAIN_IFACE, [
            ('HideGreeter', '', '', 'self.Set("com.canonical.UnityGreeter", "IsActive", False)'),
            ('ShowGreeter', '', '', 'self.Set("com.canonical.UnityGreeter", "IsActive", True)')
        ]
    )
    mock.AddProperties(
        MAIN_IFACE,
        dbus.Dictionary({
            'IsActive': parameters.get('IsActive', False),
        }, signature='sv')
    )

