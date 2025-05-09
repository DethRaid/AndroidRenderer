cmake_minimum_required(VERSION 3.22.1)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(SAH_USE_FFX_CACAO ON CACHE BOOL "" FORCE)

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

# Address sanitizer please save me
# set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /fsanitize=address")
# set(CMAKE_LINKER_FLAGS_DEBUG "${CMAKE_LINKER_FLAGS_DEBUG} /fsanitize=address")

# Include the shared renderer

set(SAH_USE_FFX 1 CACHE BOOL "" FORCE)
set(SAH_USE_STREAMLINE 1 CACHE BOOL "" FORCE)
set(SAH_USE_XESS 1 CACHE BOOL "" FORCE)
include(${CMAKE_CURRENT_LIST_DIR}/../RenderCore/SahCore.cmake)

# Win32 dependencies

include(${CMAKE_CURRENT_LIST_DIR}/extern/extern.cmake)

# This thing

set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/../cmake;")

set(SAH_SHADER_DIR ${CMAKE_CURRENT_LIST_DIR}/../RenderCore/shaders)

file(GLOB_RECURSE SHADERS CONFIGURE_DEPENDS
        ${SAH_SHADER_DIR}/*.vert
        ${SAH_SHADER_DIR}/*.geom
        ${SAH_SHADER_DIR}/*.frag
        ${SAH_SHADER_DIR}/*.comp
        )

file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS 
    ${CMAKE_CURRENT_LIST_DIR}/src/*.cpp 
    ${CMAKE_CURRENT_LIST_DIR}/src/*.hpp
    )

add_executable(SahWindows ${SOURCES})

set(SHADER_INCLUDE_DIR ${CMAKE_CURRENT_LIST_DIR}/../RenderCore)

set(EXTERN_DIR ${SHADER_INCLUDE_DIR}/extern)

add_custom_target(compile_shaders
    python ${SAH_TOOLS_DIR}/compile_shaders.py ${SAH_SHADER_DIR} ${CMAKE_BINARY_DIR}/shaders
)
add_dependencies(SahWindows compile_shaders)

target_link_libraries(SahWindows PUBLIC
        SahCore
)
    
#######################
# Generate VS filters #
#######################
foreach(source IN LISTS SOURCES)
    get_filename_component(source_path "${source}" PATH)
    string(REPLACE "${CMAKE_CURRENT_LIST_DIR}/" "" source_path_relative "${source_path}")
    string(REPLACE "/" "\\" source_path_msvc "${source_path_relative}")
    source_group("${source_path_msvc}" FILES "${source}")
endforeach()

set_property(DIRECTORY ${CMAKE_PROJECT_DIR} PROPERTY VS_STARTUP_PROJECT SahWindows)
