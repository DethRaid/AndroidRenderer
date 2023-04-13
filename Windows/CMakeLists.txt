cmake_minimum_required(VERSION 3.22.1)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

project(sah_renderer)

# Oh whoops the internet died
execute_process(
        COMMAND ping www.google.com -n 2
        ERROR_QUIET
        RESULT_VARIABLE NO_CONNECTION
)

if(NOT NO_CONNECTION EQUAL 0)
        set(FETCHCONTENT_FULLY_DISCONNECTED ON)
        message(WARNING "FetchContent offline mode: requires an already populated _deps")
else()
        set(FETCHCONTENT_FULLY_DISCONNECTED OFF)
endif()

# Include the shared renderer

include(${CMAKE_CURRENT_LIST_DIR}/../RenderCore/SahCore.cmake)

# Win32 dependencies

include(${CMAKE_CURRENT_LIST_DIR}/extern/extern.cmake)

# This thing

set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/../cmake;")
include(glsl-shaders)

set(SHADER_DIR ${CMAKE_CURRENT_LIST_DIR}/../RenderCore/shaders)

file(GLOB_RECURSE SHADERS CONFIGURE_DEPENDS
        ${SHADER_DIR}/*.vert
        ${SHADER_DIR}/*.geom
        ${SHADER_DIR}/*.frag
        ${SHADER_DIR}/*.comp
        )

file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS 
    ${CMAKE_CURRENT_LIST_DIR}/src/*.cpp 
    ${CMAKE_CURRENT_LIST_DIR}/src/*.hpp
    )

add_executable(SahWindows ${SOURCES})

set(SHADER_INCLUDE_DIR ${CMAKE_CURRENT_LIST_DIR}/../RenderCore)

set(EXTERN_DIR ${SHADER_INCLUDE_DIR}/extern)

target_glsl_shaders(SahWindows
        PRIVATE ${SHADERS}
        COMPILE_OPTIONS --target-env vulkan1.3 -g -Od -I${SHADER_INCLUDE_DIR} -I${SHADER_DIR} -I${EXTERN_DIR})

target_link_libraries(SahWindows PUBLIC
        SahCore)

add_custom_target(gltf_compression
        python ${CMAKE_CURRENT_LIST_DIR}/../Tools/optimize_gltf.py ${CMAKE_CURRENT_LIST_DIR}/../Windows/build/Debug
)
add_dependencies(SahWindows gltf_compression)
    
#######################
# Generate VS filters #
#######################
foreach(source IN LISTS SOURCES)
    get_filename_component(source_path "${source}" PATH)
    string(REPLACE "${CMAKE_CURRENT_LIST_DIR}/" "" source_path_relative "${source_path}")
    string(REPLACE "/" "\\" source_path_msvc "${source_path_relative}")
    source_group("${source_path_msvc}" FILES "${source}")
endforeach()

