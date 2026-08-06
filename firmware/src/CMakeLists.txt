# hal_sdl.cpp is guarded out under #ifndef ESP_PLATFORM, so it's safe to
# include unconditionally here alongside the ESP32/UI sources.
FILE(GLOB_RECURSE app_sources ${CMAKE_SOURCE_DIR}/src/*.*)

idf_component_register(SRCS ${app_sources} INCLUDE_DIRS "." "ui")
