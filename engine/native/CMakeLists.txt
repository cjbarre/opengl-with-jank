cmake_minimum_required(VERSION 3.24)
project(engine_native LANGUAGES C CXX)

# lein-jank's macOS sandbox can block sw_vers from reading the system version
# plist. CMake then omits Darwin's rpath flag, although our supported macOS
# versions all support it. Keep native builds sandboxed and restore that flag.
if(APPLE AND NOT CMAKE_SHARED_LIBRARY_RUNTIME_C_FLAG)
  set(CMAKE_SHARED_LIBRARY_RUNTIME_C_FLAG "-Wl,-rpath,")
  set(CMAKE_SHARED_LIBRARY_RUNTIME_CXX_FLAG "-Wl,-rpath,")
endif()

set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_INSTALL_NAME_DIR "@rpath")
set(CMAKE_BUILD_WITH_INSTALL_NAME_DIR ON)
if(APPLE)
  set(CMAKE_INSTALL_RPATH "@loader_path")
endif()
set(ozz_build_samples OFF CACHE BOOL "" FORCE)
set(ozz_build_howtos OFF CACHE BOOL "" FORCE)
set(ozz_build_tools OFF CACHE BOOL "" FORCE)
set(ozz_build_tests OFF CACHE BOOL "" FORCE)
set(ozz_build_postfix OFF CACHE BOOL "" FORCE)
add_subdirectory(../third_party/ozz-animation ozz)

foreach(lib stb_all cgltf enet)
  add_library(${lib} ${lib}.c)
  target_include_directories(${lib} PRIVATE ../include)
  install(TARGETS ${lib} DESTINATION lib)
endforeach()
if(UNIX AND NOT APPLE)
  target_link_libraries(stb_all PRIVATE m)
endif()

install(DIRECTORY ../include/ ../libs/glm/glm DESTINATION include)
