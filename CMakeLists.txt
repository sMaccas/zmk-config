if((NOT CONFIG_ZMK_SPLIT) OR CONFIG_ZMK_SPLIT_ROLE_CENTRAL)
  target_sources_ifdef(CONFIG_ZMK_BEHAVIOR_MOMENTARY_LAYER_LOCK
                       app PRIVATE src/behaviors/behavior_momentary_layer_lock.c)
endif()

# Per-board diagnostics. Compiled on both halves so the peripheral can
# track its own reset counters; widgets read the public API from the
# include/ tree exposed via vista508/CMakeLists.txt.
target_include_directories(app PRIVATE include)
target_sources(app PRIVATE src/diag/reset_reason.c)
