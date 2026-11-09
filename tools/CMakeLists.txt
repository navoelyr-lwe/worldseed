# Worldseed Engine — developer tools. Not part of the user release package.

add_executable(ws_smoke ws_smoke.c)
target_link_libraries(ws_smoke PRIVATE worldseed_core)

add_executable(ws_bench ws_bench.c)
target_link_libraries(ws_bench PRIVATE worldseed_core)
