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
