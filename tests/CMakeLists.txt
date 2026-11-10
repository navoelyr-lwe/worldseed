# Worldseed Engine — unit tests.

add_executable(ws_test_core test_core.c)
target_link_libraries(ws_test_core PRIVATE worldseed_core)

add_executable(ws_test_world test_world.c)
target_link_libraries(ws_test_world PRIVATE worldseed_core)

add_executable(ws_test_fuzz test_fuzz.c)
target_link_libraries(ws_test_fuzz PRIVATE worldseed_core)

add_executable(ws_test_render test_render.c)
target_link_libraries(ws_test_render PRIVATE worldseed_core)

add_executable(ws_test_world_env test_world_env.c)
target_link_libraries(ws_test_world_env PRIVATE worldseed_core)

add_executable(ws_test_physics test_physics.c)
target_link_libraries(ws_test_physics PRIVATE worldseed_core)

add_executable(ws_test_life test_life.c)
target_link_libraries(ws_test_life PRIVATE worldseed_core)

add_executable(ws_test_agent test_agent.c)
target_link_libraries(ws_test_agent PRIVATE worldseed_core)

add_executable(ws_test_ecology test_ecology.c)
target_link_libraries(ws_test_ecology PRIVATE worldseed_core)

add_executable(ws_test_world_host test_world_host.c)
target_link_libraries(ws_test_world_host PRIVATE worldseed_core)

add_executable(ws_test_cinema test_cinema.c)
target_link_libraries(ws_test_cinema PRIVATE worldseed_core)

add_executable(ws_test_gpu test_gpu.c)
target_link_libraries(ws_test_gpu PRIVATE worldseed_core)

add_executable(ws_test_font test_font.c)
target_link_libraries(ws_test_font PRIVATE worldseed_core)

add_executable(ws_test_session test_session.c)
target_link_libraries(ws_test_session PRIVATE worldseed_core)

add_executable(ws_test_hud test_hud.c)
target_link_libraries(ws_test_hud PRIVATE worldseed_core)

add_executable(ws_test_live test_live.c
  ${CMAKE_SOURCE_DIR}/src/app/ws_live.c
  ${CMAKE_SOURCE_DIR}/src/app/ws_live_platform.c)
target_link_libraries(ws_test_live PRIVATE worldseed_core)

add_test(NAME core COMMAND ws_test_core)
add_test(NAME world_render COMMAND ws_test_world)
add_test(NAME fuzz COMMAND ws_test_fuzz)
add_test(NAME render COMMAND ws_test_render)
add_test(NAME world_env COMMAND ws_test_world_env)
add_test(NAME physics COMMAND ws_test_physics)
add_test(NAME life COMMAND ws_test_life)
add_test(NAME agent COMMAND ws_test_agent)
add_test(NAME ecology COMMAND ws_test_ecology)
add_test(NAME world_host COMMAND ws_test_world_host)
add_test(NAME cinema COMMAND ws_test_cinema)
add_test(NAME gpu COMMAND ws_test_gpu)
add_test(NAME font COMMAND ws_test_font)
add_test(NAME session COMMAND ws_test_session)
add_test(NAME hud COMMAND ws_test_hud)
add_test(NAME live COMMAND ws_test_live)

# The OpenCL ICD (pocl in CI) keeps runtime caches it never frees; those
# allocations are in the driver, not our code. Disable leak detection for the
# gpu test so ASan still catches memory errors in our code without failing on
# the driver's own state. Harmless in non-sanitizer builds.
set_tests_properties(gpu PROPERTIES ENVIRONMENT "ASAN_OPTIONS=detect_leaks=0")

# Evidence-manifest tool: hashing a real file must succeed and emit the summary
# line; a missing file must fail (exit non-zero), so a package that silently
# dropped an artefact is caught. The tool target is defined under tools/.
if(WS_BUILD_TOOLS)
  add_test(NAME manifest_ok
    COMMAND ws_manifest --out manifest_ok.txt --version test
            ${CMAKE_SOURCE_DIR}/CMakeLists.txt)
  set_tests_properties(manifest_ok PROPERTIES
    PASS_REGULAR_EXPRESSION "WS_MANIFEST_OK files=1")
  add_test(NAME manifest_missing
    COMMAND ws_manifest --out manifest_missing.txt --version test
            ${CMAKE_SOURCE_DIR}/this-file-does-not-exist.xyz)
  set_tests_properties(manifest_missing PROPERTIES WILL_FAIL TRUE)
endif()
