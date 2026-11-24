# Platform-independent timer logic (also built by `pio test -e native`).
idf_component_register(
    SRCS "src/brush_core.c" "src/brush_i18n.c" "src/brush_oralb.c" "src/brush_scene.c"
    INCLUDE_DIRS "src"
)
