# Worldseed common compiler flags (ISO C17).
# These are development-time flags; they do not add product runtime dependencies.

function(ws_apply_common_flags)
  add_library(ws_common_flags INTERFACE)
  target_compile_features(ws_common_flags INTERFACE c_std_17)

  if(MSVC)
    target_compile_options(ws_common_flags INTERFACE /W4)
    if(WS_WARNINGS_AS_ERRORS)
      target_compile_options(ws_common_flags INTERFACE /WX)
    endif()
    target_compile_definitions(ws_common_flags INTERFACE
      _CRT_SECURE_NO_WARNINGS
      _CRT_NONSTDC_NO_DEPRECATE)
  else()
    target_compile_options(ws_common_flags INTERFACE
      -Wall -Wextra -Wpedantic -Wshadow
      -Wcast-qual -Wpointer-arith -Wstrict-prototypes -Wmissing-prototypes
      -Wundef -Wwrite-strings)
    if(WS_WARNINGS_AS_ERRORS)
      target_compile_options(ws_common_flags INTERFACE -Werror)
    endif()
    if(WS_ENABLE_SANITIZERS)
      target_compile_options(ws_common_flags INTERFACE
        -fsanitize=address,undefined -fno-omit-frame-pointer)
      target_link_options(ws_common_flags INTERFACE
        -fsanitize=address,undefined)
    endif()
  endif()

  set(WS_COMMON_FLAGS_TARGET ws_common_flags PARENT_SCOPE)
endfunction()
