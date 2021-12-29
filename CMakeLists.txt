cmake_minimum_required(VERSION 3.10)

project(gameboy-emulator)

add_executable(${PROJECT_NAME})

target_sources(${PROJECT_NAME} PRIVATE
  src/linux_handmade.c)

target_link_libraries(${PROJECT_NAME} PRIVATE
  X11
  GL
  dl)
