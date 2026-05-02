cmake_minimum_required(VERSION 3.10)
project(ozz-retarget)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ozz-animation paths
set(OZZ_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../third_party/ozz-animation")
set(OZZ_BUILD_DIR "${OZZ_DIR}/build")

# Source files
set(SOURCES
    retarget.cc
    bone_mapper.cc
)

add_executable(ozz-retarget ${SOURCES})

target_include_directories(ozz-retarget PRIVATE
    ${OZZ_DIR}/include
    ${OZZ_DIR}/src/animation/offline/gltf/extern  # For json.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# Link ozz libraries
target_link_directories(ozz-retarget PRIVATE
    ${OZZ_BUILD_DIR}/src/animation/offline
    ${OZZ_BUILD_DIR}/src/animation/runtime
    ${OZZ_BUILD_DIR}/src/base
    ${OZZ_BUILD_DIR}/src/options
)

target_link_libraries(ozz-retarget PRIVATE
    ozz_animation_offline_r
    ozz_animation_r
    ozz_base_r
    ozz_options_r
)

# Platform-specific settings
if(APPLE)
    target_link_libraries(ozz-retarget PRIVATE "-framework Foundation")
endif()

install(TARGETS ozz-retarget DESTINATION bin)

# Test executable
add_executable(retarget_tests
    retarget_tests.cc
    bone_mapper.cc
)

target_include_directories(retarget_tests PRIVATE
    ${OZZ_DIR}/include
    ${OZZ_DIR}/src/animation/offline/gltf/extern  # For json.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_directories(retarget_tests PRIVATE
    ${OZZ_BUILD_DIR}/src/animation/offline
    ${OZZ_BUILD_DIR}/src/animation/runtime
    ${OZZ_BUILD_DIR}/src/base
)

target_link_libraries(retarget_tests PRIVATE
    ozz_animation_offline_r
    ozz_animation_r
    ozz_base_r
)

if(APPLE)
    target_link_libraries(retarget_tests PRIVATE "-framework Foundation")
endif()
