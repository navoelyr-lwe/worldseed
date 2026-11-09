# Worldseed Engine — application (user-visible preview).
# Console subsystem so --selftest/--capture output is available in CI; the
# Windows build additionally links the Win32/GDI presentation layer.

add_executable(worldseed_app
  main.c
  ws_app.c
  ws_app_platform.c)

target_link_libraries(worldseed_app PRIVATE worldseed_core)

if(WIN32)
  target_link_libraries(worldseed_app PRIVATE user32 gdi32)
endif()

set_target_properties(worldseed_app PROPERTIES OUTPUT_NAME worldseed)
