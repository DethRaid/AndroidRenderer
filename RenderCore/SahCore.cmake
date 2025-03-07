cmake_minimum_required(VERSION 3.22.1)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

project(sah_renderer)

option(SAH_USE_FFX "Whether to use AMD's FidelityFX library" ON)
option(SAH_USE_STREAMLINE "Whether to use Nvidia's Streamline library" ON)

set(SAH_TOOLS_DIR "${CMAKE_CURRENT_LIST_DIR}/../Tools")

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
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "$<TARGET_FILE_DIR:SahCore>")

# External code
# Must be after we set CMAKE_RUNTIME_OUTPUT_DIRECTORY, because Streamline needs that variable
set(EXTERN_DIR ${CMAKE_CURRENT_LIST_DIR}/extern)
include(${EXTERN_DIR}/extern.cmake)

target_compile_definitions(SahCore PUBLIC
        VK_NO_PROTOTYPES
        GLM_FORCE_DEPTH_ZERO_TO_ONE
        GLM_ENABLE_EXPERIMENTAL
        _SILENCE_STDEXT_ARR_ITERS_DEPRECATION_WARNING
        TRACY_ENABLE
        SAH_BINARY_DIR="${CMAKE_RUNTIME_OUTPUT_DIRECTORY}"
        SAH_USE_FFX=${SAH_USE_FFX}
        SAH_USE_STREAMLINE=${SAH_USE_STREAMLINE}
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
        "$ENV{VK_SDK_PATH}/Include"
        )

# set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wno-format-security")
target_link_libraries(SahCore PUBLIC
        fastgltf::fastgltf
        glm::glm-header-only
        GPUOpen::VulkanMemoryAllocator
        imgui
        KTX::ktx
        magic_enum::magic_enum
        plf_colony
        renderdoc
        slang
        spdlog::spdlog
        spirv-reflect-static
        stb
        tl::optional
        Tracy::TracyClient
        utf8cpp
        vk-bootstrap
        volk::volk_headers
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
    target_compile_options(SahCore PUBLIC 
        "/MP"
    )
endif()

if(SAH_USE_FFX)
    target_link_libraries(SahCore PUBLIC
            ffx_api
            fidelityfx
        )

    add_custom_command(TARGET SahCore POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${fidelityfx_SOURCE_DIR}/PrebuiltSignedDLL/amd_fidelityfx_vk.dll"
        $<TARGET_FILE_DIR:SahCore>)
    add_custom_command(TARGET SahCore POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${fidelityfx_SOURCE_DIR}/sdk/bin/ffx_sdk/ffx_cacao_x64d.dll"
        $<TARGET_FILE_DIR:SahCore>)
    add_custom_command(TARGET SahCore POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${fidelityfx_SOURCE_DIR}/sdk/bin/ffx_sdk/ffx_fsr3_x64d.dll"
        $<TARGET_FILE_DIR:SahCore>)
endif()
if(SAH_USE_STREAMLINE)
    target_link_libraries(SahCore PUBLIC
            streamline
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

get_target_property(SAH_INCLUDES SahCore INCLUDE_DIRECTORIES)
foreach(dir ${SAH_INCLUDES})
  message(STATUS "include='${dir}'")
endforeach()

