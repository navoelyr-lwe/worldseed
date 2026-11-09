# Worldseed Engine — core library.
# One static library keeps the first milestone simple; module boundaries are
# directory and header boundaries, not separate binaries yet.

add_library(worldseed_core STATIC
  core/ws_result.c
  core/ws_math.c
  core/ws_rng.c
  core/ws_noise.c
  core/ws_handle.c
  core/ws_hash.c
  core/ws_arena.c
  core/ws_slotmap.c
  core/ws_log.c
  core/ws_serialize.c
  world/ws_world.c
  world/ws_entity.c
  world/ws_event.c
  world/ws_snapshot.c
  world/ws_terrain.c
  world/ws_hydro.c
  world/ws_habitat.c
  world/ws_environment.c
  world/ws_vegetation.c
  physics/ws_physics.c
  physics/ws_wind.c
  life/ws_creature.c
  life/ws_quadruped.c
  render/ws_framebuffer.c
  render/ws_raster.c
  render/ws_mesh.c
  render/ws_asset.c
  render/ws_scene.c
  render/ws_showcase.c
  render/ws_env_scene.c
  render/ws_catalog.c
  render/ws_render.c
  render/ws_image.c
  render/ws_texture.c
  platform/ws_dynlib.c
  platform/ws_opencl_probe.c
  platform/ws_clock.c)

target_include_directories(worldseed_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(worldseed_core PUBLIC ${WS_COMMON_FLAGS_TARGET})

if(NOT MSVC)
  target_link_libraries(worldseed_core PUBLIC m)
  if(UNIX)
    target_link_libraries(worldseed_core PUBLIC dl)
  endif()
endif()
