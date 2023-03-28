cmake_minimum_required(VERSION 3.22.1)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

project(sah_renderer)

# External code

set(EXTERN_DIR ${CMAKE_CURRENT_LIST_DIR}/extern)

include(${EXTERN_DIR}/extern.cmake)

if(ANDROID)
    # Integrate GameActivity, refer to
    #     https://d.android.com/games/agdk/integrate-game-activity
    # for the detailed instructions.
    find_package(game-activity REQUIRED CONFIG)
endif()

# Shaders

set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/../cmake;")

set(SHADER_DIR ${CMAKE_CURRENT_LIST_DIR}/shaders)

file(GLOB_RECURSE SHADERS CONFIGURE_DEPENDS
    ${SHADER_DIR}/*.vert 
    ${SHADER_DIR}/*.geom 
    ${SHADER_DIR}/*.frag 
    ${SHADER_DIR}/*.comp
    )

# SAH Core

file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_LIST_DIR}/*.cpp ${CMAKE_CURRENT_LIST_DIR}/*.hpp)

add_library(SahCore STATIC ${SOURCES})

target_compile_definitions(SahCore PUBLIC
        VK_NO_PROTOTYPES
        GLM_FORCE_DEPTH_ZERO_TO_ONE
        # TRACY_ENABLE
        )

if(WIN32)
        target_compile_definitions(SahCore PUBLIC
                VK_USE_PLATFORM_WIN32_KHR
                NOMINMAX
                )
elseif(ANDROID)
        target_compile_definitions(SahCore PUBLIC
                VK_USE_PLATFORM_ANDROID_KHR
                )
endif()

target_include_directories(SahCore PUBLIC
        ${CMAKE_CURRENT_LIST_DIR}
        )

# set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wno-format-security")
target_link_libraries(SahCore PUBLIC
        glm
        imgui
        magic_enum::magic_enum
        spdlog
        spirv-reflect-static
        stb
        fastgltf::fastgltf
        tl::optional
        Tracy::TracyClient
        vk-bootstrap
        VulkanMemoryAllocator
        volk::volk_headers
        KTX::ktx
        )

if(ANDROID)
    target_link_libraries(SahCore PUBLIC
            game-activity::game-activity
            log
            android
    )
elseif(WIN32)
    target_link_libraries(SahCore PUBLIC
        glfw 
    )
endif()

#######################
# Generate VS filters #
#######################
foreach(source IN LISTS SOURCES)
    get_filename_component(source_path "${source}" PATH)
    string(REPLACE "${CMAKE_CURRENT_LIST_DIR}/" "" source_path_relative "${source_path}")
    string(REPLACE "/" "\\" source_path_msvc "${source_path_relative}")
    source_group("${source_path_msvc}" FILES "${source}")
endforeach()

foreach(shader IN LISTS SHADERS)
    get_filename_component(source_path "${shader}" PATH)
    string(REPLACE "${CMAKE_CURRENT_LIST_DIR}/" "" source_path_relative "${source_path}")
    string(REPLACE "/" "\\" source_path_msvc "${source_path_relative}")
    source_group("${source_path_msvc}" FILES "${shader}")
endforeach()
