cmake_minimum_required(VERSION 3.20)
project(FUBAR VERSION 1.1.16 LANGUAGES C CXX)

set(SHINE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/third_party/shine/src/lib)
add_library(shine STATIC
  ${SHINE_DIR}/bitstream.c
  ${SHINE_DIR}/huffman.c
  ${SHINE_DIR}/l3bitstream.c
  ${SHINE_DIR}/l3loop.c
  ${SHINE_DIR}/l3mdct.c
  ${SHINE_DIR}/l3subband.c
  ${SHINE_DIR}/layer3.c
  ${SHINE_DIR}/reservoir.c
  ${SHINE_DIR}/tables.c
)
target_include_directories(shine PRIVATE ${SHINE_DIR})
if(MINGW)
  target_compile_options(shine PRIVATE -std=c99 -w)
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(fubar
  src/main.cpp
  src/audio_engine.cpp
  src/wav_writer.cpp
  src/app_window.cpp
  src/web_server.cpp
  src/live_hub.cpp
  src/live_slot.cpp
  src/fubar_net.cpp
  src/live_mp3.cpp
  src/audio_safety.h
  src/live_mp3.h
  src/fubar_net.h
  src/live_hub.h
  src/live_slot.h
  src/app_paths.h
  src/vox_gate.h
  src/web_server.h
)

if(WIN32)
  target_sources(fubar PRIVATE resources/app.rc)
endif()
target_compile_definitions(fubar PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)

if(MSVC)
  target_compile_options(fubar PRIVATE /W4 /permissive- /utf-8)
  set_property(TARGET fubar PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
elseif(MINGW)
  target_compile_options(fubar PRIVATE -Wall -Wextra -Wpedantic)
  target_link_options(fubar PRIVATE -municode -static-libgcc -static-libstdc++)
endif()

target_link_libraries(fubar PRIVATE shine ole32 uuid winmm comctl32 shell32 ws2_32 winhttp)

set_target_properties(fubar PROPERTIES OUTPUT_NAME "FUBAR")

enable_testing()
add_test(NAME cli_help COMMAND fubar --cli --help)
add_test(NAME self_test COMMAND fubar --cli --self-test)
