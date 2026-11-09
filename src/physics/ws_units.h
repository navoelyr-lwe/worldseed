/* Worldseed Engine — SI units and physical constants.
 *
 * Every physics model parameter is stated in SI units (metre, kilogram,
 * second, newton, joule, pascal, kelvin) or with an explicit conversion
 * documented here. This header is the single place where unit conventions are
 * declared, so model cards (docs/02) can cite one source.
 */
#ifndef WS_PHYSICS_UNITS_H
#define WS_PHYSICS_UNITS_H

/* Standard gravity used by the environmental physics models. Earth mean value;
 * the reference test compares free fall against this exact constant. */
#define WS_GRAVITY 9.80665 /* m/s^2 */

/* Physical constants used by the environmental sub-models. Values are the
 * SI-defining or standard-atmosphere values so tests are reproducible. */
#define WS_AIR_DENSITY_SEA_LEVEL 1.225     /* kg/m^3 at 15 C, 101.325 kPa */
#define WS_WATER_DENSITY 997.0             /* kg/m^3 at ~25 C */
#define WS_WATER_SPECIFIC_HEAT 4186.0      /* J/(kg*K) */

/* Conversions. Kept as small functions so call sites read as units. */
static inline double ws_celsius_to_kelvin(double c) { return c + 273.15; }
static inline double ws_kelvin_to_celsius(double k) { return k - 273.15; }

#endif /* WS_PHYSICS_UNITS_H */
