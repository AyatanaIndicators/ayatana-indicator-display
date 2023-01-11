/*
 * Parts of this file have been taken from the Redshift project:
 * https://github.com/jonls/redshift/
 *
 * Copyright 2010 Jon Lund Steffensen
 * Copyright 2023 Robert Tari
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
 *   Jon Lund Steffensen <jonlst@gmail.com>
 *   Robert Tari <robert@tari.in>
 */

#ifndef REDSHIFT_SOLAR_H
#define REDSHIFT_SOLAR_H

#define SOLAR_CIVIL_TWILIGHT_ELEV    -6.0

double solar_elevation(double date, double lat, double lon);

#endif /* ! REDSHIFT_SOLAR_H */
