cmake_minimum_required(VERSION 3.10)
project(gla2ozz)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ozz-animation paths
set(OZZ_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../third_party/ozz-animation")
set(OZZ_BUILD_DIR "${OZZ_DIR}/build")

# Source files
set(SOURCES
    gla2ozz.cc
    gla_parser.cc
    animation_cfg_parser.cc
)

add_executable(gla2ozz ${SOURCES})

target_include_directories(gla2ozz PRIVATE
    ${OZZ_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# Link ozz libraries (static) - using _r suffix for release builds
target_link_directories(gla2ozz PRIVATE
    ${OZZ_BUILD_DIR}/src/animation/offline/tools
    ${OZZ_BUILD_DIR}/src/animation/offline/tools/json
    ${OZZ_BUILD_DIR}/src/animation/offline
    ${OZZ_BUILD_DIR}/src/animation/runtime
    ${OZZ_BUILD_DIR}/src/base
    ${OZZ_BUILD_DIR}/src/options
)

target_link_libraries(gla2ozz PRIVATE
    ozz_animation_tools_r
    json_r
    ozz_animation_offline_r
    ozz_animation_r
    ozz_base_r
    ozz_options_r
)

# Platform-specific settings
if(APPLE)
    target_link_libraries(gla2ozz PRIVATE "-framework Foundation")
endif()

install(TARGETS gla2ozz DESTINATION bin)

# Test executable
add_executable(gla_tests
    gla_tests.cc
    gla_parser.cc
    animation_cfg_parser.cc
)

target_include_directories(gla_tests PRIVATE
    ${OZZ_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_directories(gla_tests PRIVATE
    ${OZZ_BUILD_DIR}/src/animation/offline
    ${OZZ_BUILD_DIR}/src/animation/runtime
    ${OZZ_BUILD_DIR}/src/base
)

target_link_libraries(gla_tests PRIVATE
    ozz_animation_offline_r
    ozz_animation_r
    ozz_base_r
)

if(APPLE)
    target_link_libraries(gla_tests PRIVATE "-framework Foundation")
endif()
