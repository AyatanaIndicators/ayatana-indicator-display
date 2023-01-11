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

#include <math.h>
#include "solar.h"

#define RAD(x)  ((x)*(M_PI/180))
#define DEG(x)  ((x)*(180/M_PI))

/* Julian centuries since J2000.0 from Julian day */
static double
jcent_from_jd(double jd)
{
    return (jd - 2451545.0) / 36525.0;
}

/* Angular elevation at the location for the given hour angle.
   lat: Latitude of location in degrees
   decl: Declination in radians
   ha: Hour angle in radians
   Return: Angular elevation in radians */
static double
elevation_from_hour_angle(double lat, double decl, double ha)
{
    return asin(cos(ha)*cos(RAD(lat))*cos(decl) +
            sin(RAD(lat))*sin(decl));
}

/* Geometric mean anomaly of the sun.
   t: Julian centuries since J2000.0
   Return: Geometric mean anomaly in radians. */
static double
sun_geom_mean_anomaly(double t)
{
    return RAD(357.52911 + t*(35999.05029 - t*0.0001537));
}

/* Equation of center of the sun.
   t: Julian centuries since J2000.0
   Return: Center(?) in radians */
static double
sun_equation_of_center(double t)
{
    /* Use the first three terms of the equation. */
    double m = sun_geom_mean_anomaly(t);
    double c = sin(m)*(1.914602 - t*(0.004817 + 0.000014*t)) +
        sin(2*m)*(0.019993 - 0.000101*t) +
        sin(3*m)*0.000289;
    return RAD(c);
}

/* Geometric mean longitude of the sun.
   t: Julian centuries since J2000.0
   Return: Geometric mean logitude in radians. */
static double
sun_geom_mean_lon(double t)
{
    /* FIXME returned value should always be positive */
    return RAD(fmod(280.46646 + t*(36000.76983 + t*0.0003032), 360));
}

/* True longitude of the sun.
   t: Julian centuries since J2000.0
   Return: True longitude in radians */
static double
sun_true_lon(double t)
{
    double l_0 = sun_geom_mean_lon(t);
    double c = sun_equation_of_center(t);
    return l_0 + c;
}

/* Apparent longitude of the sun. (Right ascension).
   t: Julian centuries since J2000.0
   Return: Apparent longitude in radians */
static double
sun_apparent_lon(double t)
{
    double o = sun_true_lon(t);
    return RAD(DEG(o) - 0.00569 - 0.00478*sin(RAD(125.04 - 1934.136*t)));
}

/* Mean obliquity of the ecliptic
   t: Julian centuries since J2000.0
   Return: Mean obliquity in radians */
static double
mean_ecliptic_obliquity(double t)
{
    double sec = 21.448 - t*(46.815 + t*(0.00059 - t*0.001813));
    return RAD(23.0 + (26.0 + (sec/60.0))/60.0);
}

/* Corrected obliquity of the ecliptic.
   t: Julian centuries since J2000.0
   Return: Currected obliquity in radians */
static double
obliquity_corr(double t)
{
    double e_0 = mean_ecliptic_obliquity(t);
    double omega = 125.04 - t*1934.136;
    return RAD(DEG(e_0) + 0.00256*cos(RAD(omega)));
}

/* Declination of the sun.
   t: Julian centuries since J2000.0
   Return: Declination in radians */
static double
solar_declination(double t)
{
    double e = obliquity_corr(t);
    double lambda = sun_apparent_lon(t);
    return asin(sin(e)*sin(lambda));
}

/* Eccentricity of earth orbit.
   t: Julian centuries since J2000.0
   Return: Eccentricity (unitless). */
static double
earth_orbit_eccentricity(double t)
{
    return 0.016708634 - t*(0.000042037 + t*0.0000001267);
}

/* Difference between true solar time and mean solar time.
   t: Julian centuries since J2000.0
   Return: Difference in minutes */
static double
equation_of_time(double t)
{
    double epsilon = obliquity_corr(t);
    double l_0 = sun_geom_mean_lon(t);
    double e = earth_orbit_eccentricity(t);
    double m = sun_geom_mean_anomaly(t);
    double y = pow(tan(epsilon/2.0), 2.0);

    double eq_time = y*sin(2*l_0) - 2*e*sin(m) +
        4*e*y*sin(m)*cos(2*l_0) -
        0.5*y*y*sin(4*l_0) -
        1.25*e*e*sin(2*m);
    return 4*DEG(eq_time);
}

/* Julian day from Julian centuries since J2000.0 */
static double
jd_from_jcent(double t)
{
    return 36525.0*t + 2451545.0;
}

/* Solar angular elevation at the given location and time.
   t: Julian centuries since J2000.0
   lat: Latitude of location
   lon: Longitude of location
   Return: Solar angular elevation in radians */
static double
solar_elevation_from_time(double t, double lat, double lon)
{
    /* Minutes from midnight */
    double jd = jd_from_jcent(t);
    double offset = (jd - round(jd) - 0.5)*1440.0;

    double eq_time = equation_of_time(t);
    double ha = RAD((720 - offset - eq_time)/4 - lon);
    double decl = solar_declination(t);
    return elevation_from_hour_angle(lat, decl, ha);
}

/* Julian day from unix epoch */
static double
jd_from_epoch(double t)
{
    return (t / 86400.0) + 2440587.5;
}

/* Solar angular elevation at the given location and time.
   date: Seconds since unix epoch
   lat: Latitude of location
   lon: Longitude of location
   Return: Solar angular elevation in degrees */
double
solar_elevation(double date, double lat, double lon)
{
    double jd = jd_from_epoch(date);
    return DEG(solar_elevation_from_time(jcent_from_jd(jd), lat, lon));
}
