cmake_minimum_required(VERSION 3.10)
project(ozz2gltf)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ozz-animation paths
set(OZZ_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../third_party/ozz-animation")
set(OZZ_BUILD_DIR "${OZZ_DIR}/build")

# tinygltf path (header-only)
set(TINYGLTF_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../third_party/tinygltf")

# Source files
set(SOURCES
    ozz2gltf.cc
    skeleton_converter.cc
)

add_executable(ozz2gltf ${SOURCES})

target_include_directories(ozz2gltf PRIVATE
    ${OZZ_DIR}/include
    ${TINYGLTF_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# Link ozz libraries (runtime only - no offline tools needed)
target_link_directories(ozz2gltf PRIVATE
    ${OZZ_BUILD_DIR}/src/animation/runtime
    ${OZZ_BUILD_DIR}/src/base
)

target_link_libraries(ozz2gltf PRIVATE
    ozz_animation_r
    ozz_base_r
)

# Platform-specific settings
if(APPLE)
    target_link_libraries(ozz2gltf PRIVATE "-framework Foundation")
endif()

install(TARGETS ozz2gltf DESTINATION bin)
