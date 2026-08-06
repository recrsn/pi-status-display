# ESP-IDF (used only by the esp32s3 env) ignores platformio.ini's
# build_src_filter, so this glob must exclude src/emulator/* itself
# (that's the native/SDL env's sources, not this firmware's).
FILE(GLOB_RECURSE app_sources ${CMAKE_SOURCE_DIR}/src/firmware/*.*)

idf_component_register(SRCS ${app_sources})
