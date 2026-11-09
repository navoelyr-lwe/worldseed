/* Worldseed Engine — wind field for physics.
 *
 * Bridges the simulated atmosphere (docs/04, ws_environment) to the physics
 * solver's wind vector. The horizontal direction and speed come from the
 * environment's deterministic wind state; a simple power-law profile scales
 * the speed with height above the ground, the standard engineering
 * approximation for a neutrally stratified surface layer:
 *
 *     u(z) = u_ref * (z / z_ref)^alpha
 *
 * with alpha ~ 0.14 (open terrain, "Hellmann" exponent). This is an
 * approximation, not a solved boundary layer; the model card records the
 * assumption and its validity range.
 */
#ifndef WS_PHYSICS_WIND_H
#define WS_PHYSICS_WIND_H

#include "../core/ws_math.h"
#include "../world/ws_environment.h"

#define WS_WIND_SHEAR_EXPONENT 0.14 /* Hellmann exponent, open terrain */
#define WS_WIND_REFERENCE_HEIGHT 10.0 /* m; standard anemometer height */

/* Wind velocity (m/s) as a 3D vector at `height_m` above the ground, given the
 * environment's horizontal wind (2D, m/s). Deterministic and pure. */
ws_vec3 ws_wind_at_height(const ws_environment *env, double height_m);

#endif /* WS_PHYSICS_WIND_H */
