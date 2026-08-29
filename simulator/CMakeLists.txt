cmake_minimum_required(VERSION 3.16)
project(desk_assistant_simulator LANGUAGES C CXX)

set(CMAKE_C_STANDARD 99)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(LVGL_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../.pio/libdeps/esp32-cyd/lvgl" CACHE PATH "LVGL v8 source directory")
set(ARDUINOJSON_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../.pio/libdeps/esp32-cyd/ArduinoJson/src" CACHE PATH "ArduinoJson include directory")

if(NOT EXISTS "${LVGL_DIR}/lvgl.h")
  message(FATAL_ERROR "LVGL não encontrado em ${LVGL_DIR}. Execute primeiro: pio run -e esp32-cyd")
endif()
if(NOT EXISTS "${ARDUINOJSON_DIR}/ArduinoJson.h")
  message(FATAL_ERROR "ArduinoJson não encontrado em ${ARDUINOJSON_DIR}. Execute primeiro: pio run -e esp32-cyd")
endif()

find_package(PkgConfig REQUIRED)
pkg_check_modules(SDL2 REQUIRED IMPORTED_TARGET sdl2)

file(GLOB_RECURSE LVGL_SOURCES CONFIGURE_DEPENDS "${LVGL_DIR}/src/*.c")
add_library(lvgl STATIC ${LVGL_SOURCES})
target_include_directories(lvgl PUBLIC
  "${LVGL_DIR}"
  "${LVGL_DIR}/src"
  "${CMAKE_CURRENT_SOURCE_DIR}"
)
target_compile_definitions(lvgl PUBLIC LV_CONF_INCLUDE_SIMPLE)

add_executable(card_simulator main.cpp)
target_include_directories(card_simulator PRIVATE "${ARDUINOJSON_DIR}")
target_link_libraries(card_simulator PRIVATE lvgl PkgConfig::SDL2)
target_compile_options(card_simulator PRIVATE -Wall -Wextra -Wpedantic)
