idf_component_register(
    SRC_DIRS "."
    INCLUDE_DIRS "."
    REQUIRES esp_timer
)
target_compile_options(${COMPONENT_LIB} PRIVATE -DESP32 -Wno-unused-variable -Wno-unused-function -Wno-error)
