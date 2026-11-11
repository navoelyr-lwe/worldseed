# Worldseed Engine — developer tools. Not part of the user release package.

add_executable(ws_smoke ws_smoke.c)
target_link_libraries(ws_smoke PRIVATE worldseed_core)

add_executable(ws_bench ws_bench.c)
target_link_libraries(ws_bench PRIVATE worldseed_core)

add_executable(ws_golden ws_golden.c)
target_link_libraries(ws_golden PRIVATE worldseed_core)

add_executable(ws_imgdiff ws_imgdiff.c)
target_link_libraries(ws_imgdiff PRIVATE worldseed_core)

add_executable(ws_assetgen ws_assetgen.c)
target_link_libraries(ws_assetgen PRIVATE worldseed_core)

add_executable(ws_audio ws_audio.c)
target_link_libraries(ws_audio PRIVATE worldseed_core)

add_executable(ws_gpu_check ws_gpu_check.c)
target_link_libraries(ws_gpu_check PRIVATE worldseed_core)

add_executable(ws_perf ws_perf.c)
target_link_libraries(ws_perf PRIVATE worldseed_core)

add_executable(ws_manifest ws_manifest.c)
target_link_libraries(ws_manifest PRIVATE worldseed_core)

add_executable(ws_quality ws_quality.c)
target_link_libraries(ws_quality PRIVATE worldseed_core)
