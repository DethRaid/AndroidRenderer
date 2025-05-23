# Download external dependencies

include(FetchContent)

set(VULKAN_DIR "$ENV{VULKAN_SDK}")

set(BUILD_TESTING OFF CACHE BOOL "" FORCE)

set(OPTIONAL_BUILD_PACKAGE OFF CACHE BOOL "" FORCE)
set(OPTIONAL_BUILD_TESTS OFF CACHE BOOL "" FORCE)

set(SPIRV_REFLECT_EXECUTABLE OFF CACHE BOOL "" FORCE)
set(SPIRV_REFLECT_EXAMPLES OFF CACHE BOOL "" FORCE)
set(SPIRV_REFLECT_STATIC_LIB ON CACHE BOOL "" FORCE)

set(VMA_STATIC_VULKAN_FUNCTIONS OFF CACHE BOOL "" FORCE)
set(VMA_DYNAMIC_VULKAN_FUNCTIONS ON CACHE BOOL "" FORCE)

set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE) 
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)

set(FASTGLTF_ENABLE_TESTS OFF CACHE BOOL "" FORCE)
set(FASTGLTF_ENABLE_EXAMPLES OFF CACHE BOOL "" FORCE)

set(STREAMLINE_FEATURE_DLSS_SR ON CACHE BOOL "" FORCE)
set(STREAMLINE_FEATURE_DLSS_RR ON CACHE BOOL "" FORCE)

set(CAULDRON_VK ON CACHE BOOL "" FORCE)

message(STATUS "CMAKE_RUNTIME_OUTPUT_DIRECTORY=${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")

FetchContent_Declare(
        eastl
        GIT_REPOSITORY  https://github.com/electronicarts/EASTL.git
        GIT_TAG         3.21.23 
)
FetchContent_Declare(
        glm
        GIT_REPOSITORY  https://github.com/g-truc/glm.git
        GIT_TAG         1.0.0
)
FetchContent_Declare(
        fetch_magic_enum
        GIT_REPOSITORY  https://github.com/Neargye/magic_enum.git
        GIT_TAG         v0.9.5
)
FetchContent_Declare(
        spdlog
        GIT_REPOSITORY  https://github.com/gabime/spdlog.git
        GIT_TAG         v1.13.0
)
FetchContent_Declare(
        fetch_spirv_reflect
        GIT_REPOSITORY  https://github.com/KhronosGroup/SPIRV-Reflect.git
        GIT_TAG         vulkan-sdk-1.4.304.0
)
FetchContent_Declare(
        fetch_fastgltf
        GIT_REPOSITORY  https://github.com/spnda/fastgltf.git
        GIT_TAG         057a535f26c8ec2cbe234cb80cb5f74df908d9bd
)
FetchContent_Declare(
        tl_optional
        GIT_REPOSITORY  https://github.com/TartanLlama/optional.git
        GIT_TAG         c28fcf74d207fc667c4ed3dbae4c251ea551c8c1
)
FetchContent_Declare(
        fetch_tracy
        GIT_REPOSITORY  https://github.com/wolfpld/tracy.git
        GIT_TAG         v0.11.1
)
FetchContent_Declare(
        utf8cpp
        GIT_REPOSITORY https://github.com/nemtrif/utfcpp.git
        GIT_TAG        v4.0.6
)
FetchContent_Declare(
        fetch_volk
        GIT_REPOSITORY  https://github.com/zeux/volk.git
        GIT_TAG         7121910043955154c585ebb63de2e7d63aefa489
)
FetchContent_Declare(
        vk-bootstrap
        GIT_REPOSITORY  https://github.com/charles-lunarg/vk-bootstrap.git
        GIT_TAG         c73b793387e739b9abdf952c52cde40d1b0e7db0
        #GIT_REPOSITORY  https://github.com/DethRaid/vk-bootstrap.git
        #GIT_TAG         vk_14_features
)
FetchContent_Declare(
        fetch_vma
        GIT_REPOSITORY  https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator.git
        GIT_TAG         38627f4e37d7a9b13214fd267ec60e0e877e3997
)

FetchContent_MakeAvailable(
        eastl
        glm
        spdlog
        fetch_fastgltf
        tl_optional
        fetch_magic_enum
        fetch_spirv_reflect
        utf8cpp
        fetch_tracy
        fetch_vma
        vk-bootstrap
        fetch_volk)

if(ANDROID)
        FetchContent_Declare(
                libadrenotools
                GIT_REPOSITORY  https://github.com/bylaws/libadrenotools.git
                GIT_TAG         8fae8ce254dfc1344527e05301e43f37dea2df80
        )
        FetchContent_MakeAvailable(libadrenotools)
endif()

if(SAH_USE_STREAMLINE)
    FetchContent_Declare(
            streamline
            GIT_REPOSITORY      https://github.com/NVIDIAGameWorks/Streamline.git
            GIT_TAG             v2.7.2
    )
    FetchContent_MakeAvailable(streamline)
endif()

if(SAH_USE_FFX)
    FetchContent_Declare(
            fidelityfx
            DOWNLOAD_EXTRACT_TIMESTAMP OFF
            URL             https://github.com/GPUOpen-LibrariesAndSDKs/FidelityFX-SDK/releases/download/v1.1.3/FidelityFX-SDK-v1.1.3.zip
    )
    FetchContent_GetProperties(fidelityfx)
    if(fidelityfx_POPULATED)
        message("fidelityfx automatically populated")
    else()
        FetchContent_Populate(fidelityfx)

        add_library(ffx_api INTERFACE)
        target_include_directories(ffx_api INTERFACE
                "${fidelityfx_SOURCE_DIR}/ffx-api/include"
                )
        target_link_directories(ffx_api INTERFACE
            ${fidelityfx_SOURCE_DIR}/PrebuiltSignedDLL
        )
        target_link_libraries(ffx_api INTERFACE
            amd_fidelityfx_vk)

        add_library(fidelityfx INTERFACE)
        target_include_directories(fidelityfx INTERFACE
                "${fidelityfx_SOURCE_DIR}/sdk/include"
                )
        target_link_directories(fidelityfx INTERFACE
                "${fidelityfx_SOURCE_DIR}/sdk/bin/ffx_sdk"
                )
        target_link_libraries(fidelityfx INTERFACE
            ffx_backend_vk_x64d
            ffx_cacao_x64d
            ffx_fsr3_x64d
        )
    endif()
endif()

if(SAH_USE_XESS)
        FetchContent_Declare(
                xess
                GIT_REPOSITORY  https://github.com/intel/xess.git     
                GIT_TAG         v2.0.1
        )
        FetchContent_GetProperties(xess)
        if(xess_POPULATED)
                message("XeSS automatically populated")
        else()
                FetchContent_Populate(xess)

                add_library(xess INTERFACE)
                target_include_directories(xess INTERFACE 
                        ${xess_SOURCE_DIR}/inc
                )
                target_link_directories(xess INTERFACE
                        ${xess_SOURCE_DIR}/lib
                )
                target_link_libraries(xess INTERFACE 
                        libxess
                )
        endif()
endif()

FetchContent_Declare(
        fetch_imgui
        GIT_REPOSITORY  https://github.com/ocornut/imgui.git
        GIT_TAG         e13913ed572dbd95dedf840a94db5f27a1fdf2a5
)
FetchContent_GetProperties(fetch_imgui)
if(fetch_imgui_POPULATED)
    message("Dear ImGUI automatically populated")
else()
    FetchContent_Populate(fetch_imgui)
    add_library(imgui STATIC
            ${fetch_imgui_SOURCE_DIR}/imgui.cpp
            ${fetch_imgui_SOURCE_DIR}/imgui_demo.cpp
            ${fetch_imgui_SOURCE_DIR}/imgui_draw.cpp
            ${fetch_imgui_SOURCE_DIR}/imgui_tables.cpp
            ${fetch_imgui_SOURCE_DIR}/imgui_widgets.cpp
            ${fetch_imgui_SOURCE_DIR}/misc/cpp/imgui_stdlib.cpp
            )
    target_include_directories(imgui PUBLIC
            ${fetch_imgui_SOURCE_DIR}
            ${fetch_imgui_SOURCE_DIR}/misc/cpp
            )
endif()

FetchContent_Declare(
        plf_colony
        GIT_REPOSITORY  https://github.com/mattreecebentley/plf_colony.git
        GIT_TAG         5aeb1e6dbc7686f9f09eef685e57dc6acfe616c7
)
FetchContent_GetProperties(plf_colony)
if(plf_colony_POPULATED)
    message("plf::colony automatically populated")
else()
    FetchContent_Populate(plf_colony)
    add_library(plf_colony INTERFACE)
    target_include_directories(plf_colony INTERFACE
            ${plf_colony_SOURCE_DIR}
            )
endif()

FetchContent_Declare(
        fetch_stb
        GIT_REPOSITORY  https://github.com/nothings/stb.git
        GIT_TAG         8b5f1f37b5b75829fc72d38e7b5d4bcbf8a26d55
)
FetchContent_GetProperties(fetch_stb)
if(fetch_stb_POPULATED)
    message("STB automatically populated")
else()
    FetchContent_Populate(fetch_stb)
    add_library(stb INTERFACE)
    target_include_directories(stb INTERFACE ${fetch_stb_SOURCE_DIR})
endif()

# We only use GLFW on Windows
if(WIN32)
    FetchContent_Declare(
            fetch_glfw
            GIT_REPOSITORY  https://github.com/glfw/glfw.git
            GIT_TAG         3.3.8
    )
    FetchContent_MakeAvailable(fetch_glfw)
endif()

# libKTX
# The build script has a hard dependency on Bash, so we have this malarkey for the precompiled binaries
if(ANDROID)
    set(ktx_DIR "${CMAKE_CURRENT_LIST_DIR}/libktx/arm64-v8a/lib/cmake/ktx")
elseif(WIN32)
    set(ktx_DIR "D:/Program Files/KTX-Software/lib/cmake/ktx")
endif()

find_package(ktx REQUIRED)

# Slang
add_library(slang INTERFACE)
target_include_directories(slang INTERFACE "${CMAKE_CURRENT_LIST_DIR}/slang/include")
target_link_directories(slang INTERFACE "${CMAKE_CURRENT_LIST_DIR}/swlang/lib/windows-x64/release")
target_link_libraries(slang INTERFACE slang)

# RenderDoc API, from https://github.com/baldurk/renderdoc/blob/v1.x/renderdoc/api/app/renderdoc_app.h
add_library(renderdoc INTERFACE)
target_include_directories(renderdoc INTERFACE ${CMAKE_CURRENT_LIST_DIR}/renderdoc/include)
