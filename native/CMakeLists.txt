# Builds the native cat. Windows: MSVC or MinGW. Linux also has a plain Makefile.
cmake_minimum_required(VERSION 3.16)
project(gato_de_mesa CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

file(GLOB SHARED src/core/*.cpp src/app/*.cpp src/render/*.cpp)

if(WIN32)
  # one .exe with no runtime DLLs to install
  set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
  add_executable(gato-de-mesa WIN32 ${SHARED} src/platform/windows/main.cpp)
  target_compile_definitions(gato-de-mesa PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
  target_link_libraries(gato-de-mesa PRIVATE user32 gdi32)
  if(MSVC)
    target_compile_options(gato-de-mesa PRIVATE /utf-8 /W3)
    target_link_options(gato-de-mesa PRIVATE /ENTRY:wWinMainCRTStartup)
  else()
    target_link_options(gato-de-mesa PRIVATE -municode -static)
  endif()
else()
  find_package(X11 REQUIRED)
  file(GLOB LINUX src/platform/linux/*.cpp)
  add_executable(gato-de-mesa ${SHARED} ${LINUX})
  target_link_libraries(gato-de-mesa PRIVATE X11::X11 X11::Xext)
endif()

# the sprites travel next to the program
add_custom_command(TARGET gato-de-mesa POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/assets $<TARGET_FILE_DIR:gato-de-mesa>/assets)
