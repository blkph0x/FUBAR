cmake_minimum_required(VERSION 3.20)
project(AudioVox VERSION 1.0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(audiovox
  src/main.cpp
  src/audio_engine.cpp
  src/wav_writer.cpp
  src/app_window.cpp
  src/vox_gate.h
)

target_include_directories(audiovox PRIVATE src)
target_compile_definitions(audiovox PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)

if(MSVC)
  target_compile_options(audiovox PRIVATE /W4 /permissive- /utf-8)
  set_property(TARGET audiovox PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
elseif(MINGW)
  target_compile_options(audiovox PRIVATE -Wall -Wextra -Wpedantic)
  target_link_options(audiovox PRIVATE -municode -static-libgcc -static-libstdc++)
endif()

target_link_libraries(audiovox PRIVATE ole32 uuid winmm comctl32 shell32)

set_target_properties(audiovox PROPERTIES OUTPUT_NAME "AudioVox")

enable_testing()
add_test(NAME cli_help COMMAND audiovox --help)
add_test(NAME self_test COMMAND audiovox --self-test)
