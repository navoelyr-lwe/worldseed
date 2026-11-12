# Worldseed Engine — core library.
# One static library keeps the first milestone simple; module boundaries are
# directory and header boundaries, not separate binaries yet.

# Embed the OpenCL C kernel source into a generated C file so the product ships
# no OpenCL runtime and the kernel travels inside the executable (ADR-001).
set(WS_KERNEL_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../kernels)
set(WS_KERNEL_GEN ${CMAKE_CURRENT_BINARY_DIR}/ws_kernel_ws_raster.c)
add_custom_command(
  OUTPUT ${WS_KERNEL_GEN}
  COMMAND ${CMAKE_COMMAND}
          -DKERNEL_IN=${WS_KERNEL_DIR}/ws_raster.cl
          -DKERNEL_OUT=${WS_KERNEL_GEN}
          -DKERNEL_NAME=ws_raster
          -P ${CMAKE_CURRENT_SOURCE_DIR}/../cmake/embed_kernel.cmake
  DEPENDS ${WS_KERNEL_DIR}/ws_raster.cl ${CMAKE_CURRENT_SOURCE_DIR}/../cmake/embed_kernel.cmake
  COMMENT "Embedding OpenCL kernel ws_raster.cl")
add_custom_target(ws_kernel_gen DEPENDS ${WS_KERNEL_GEN})

set(WS_KERNEL_TILED_GEN ${CMAKE_CURRENT_BINARY_DIR}/ws_kernel_ws_raster_tiled.c)
add_custom_command(
  OUTPUT ${WS_KERNEL_TILED_GEN}
  COMMAND ${CMAKE_COMMAND}
          -DKERNEL_IN=${WS_KERNEL_DIR}/ws_raster_tiled.cl
          -DKERNEL_OUT=${WS_KERNEL_TILED_GEN}
          -DKERNEL_NAME=ws_raster_tiled
          -P ${CMAKE_CURRENT_SOURCE_DIR}/../cmake/embed_kernel.cmake
  DEPENDS ${WS_KERNEL_DIR}/ws_raster_tiled.cl ${CMAKE_CURRENT_SOURCE_DIR}/../cmake/embed_kernel.cmake
  COMMENT "Embedding OpenCL kernel ws_raster_tiled.cl")
add_custom_target(ws_kernel_tiled_gen DEPENDS ${WS_KERNEL_TILED_GEN})

set(WS_KERNEL_SCENE_GEN ${CMAKE_CURRENT_BINARY_DIR}/ws_kernel_ws_scene.c)
add_custom_command(
  OUTPUT ${WS_KERNEL_SCENE_GEN}
  COMMAND ${CMAKE_COMMAND}
          -DKERNEL_IN=${WS_KERNEL_DIR}/ws_scene.cl
          -DKERNEL_OUT=${WS_KERNEL_SCENE_GEN}
          -DKERNEL_NAME=ws_scene
          -P ${CMAKE_CURRENT_SOURCE_DIR}/../cmake/embed_kernel.cmake
  DEPENDS ${WS_KERNEL_DIR}/ws_scene.cl ${CMAKE_CURRENT_SOURCE_DIR}/../cmake/embed_kernel.cmake
  COMMENT "Embedding OpenCL kernel ws_scene.cl")
add_custom_target(ws_kernel_scene_gen DEPENDS ${WS_KERNEL_SCENE_GEN})

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
  core/ws_sched.c
  world/ws_world.c
  world/ws_entity.c
  world/ws_event.c
  world/ws_snapshot.c
  world/ws_terrain.c
  world/ws_hydro.c
  world/ws_habitat.c
  world/ws_environment.c
  world/ws_vegetation.c
  world/ws_region.c
  world/ws_world_host.c
  physics/ws_physics.c
  physics/ws_wind.c
  life/ws_creature.c
  life/ws_quadruped.c
  life/ws_perception.c
  life/ws_organism.c
  life/ws_agent.c
  life/ws_species.c
  life/ws_ecology.c
  life/ws_population.c
  life/ws_life_event.c
  cinema/ws_director.c
  audio/ws_audio.c
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
  render/ws_quality.c
  render/ws_motion.c
  render/ws_texture.c
  ui/ws_font.c
  ui/ws_session.c
  ui/ws_hud.c
  platform/ws_dynlib.c
  platform/ws_fs.c
  platform/ws_settings.c
  platform/ws_opencl_probe.c
  platform/ws_threads.c
  platform/ws_hardware.c
  platform/ws_clock.c
  gpu/ws_opencl.c
  gpu/ws_tilebin.c
  render/ws_gpu_scene.c
  ${WS_KERNEL_GEN}
  ${WS_KERNEL_TILED_GEN}
  ${WS_KERNEL_SCENE_GEN})

add_dependencies(worldseed_core ws_kernel_gen ws_kernel_tiled_gen ws_kernel_scene_gen)

target_include_directories(worldseed_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(worldseed_core PUBLIC ${WS_COMMON_FLAGS_TARGET})

if(NOT MSVC)
  target_link_libraries(worldseed_core PUBLIC m)
  if(UNIX)
    target_link_libraries(worldseed_core PUBLIC dl)
  endif()
  find_package(Threads REQUIRED)
  target_link_libraries(worldseed_core PUBLIC Threads::Threads)
endif()
