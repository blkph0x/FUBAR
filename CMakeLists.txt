cmake_minimum_required(VERSION 3.20)
project(FUBAR VERSION 1.1.5 LANGUAGES CXX)

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
  src/audio_safety.h
  src/live_hub.h
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

target_link_libraries(fubar PRIVATE ole32 uuid winmm comctl32 shell32 ws2_32)

set_target_properties(fubar PROPERTIES OUTPUT_NAME "FUBAR")

enable_testing()
add_test(NAME cli_help COMMAND fubar --cli --help)
add_test(NAME self_test COMMAND fubar --cli --self-test)
