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

add_test(NAME core COMMAND ws_test_core)
add_test(NAME world_render COMMAND ws_test_world)
add_test(NAME fuzz COMMAND ws_test_fuzz)
add_test(NAME render COMMAND ws_test_render)
add_test(NAME world_env COMMAND ws_test_world_env)
