# Download external dependencies

include(FetchContent)

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
        GIT_TAG         vulkan-sdk-1.3.275.0
)
FetchContent_Declare(
        fetch_fastgltf
        GIT_REPOSITORY  https://github.com/spnda/fastgltf.git
        GIT_TAG         aa43588c42b78bc188fdd36124f1d1491e04a680
)
FetchContent_Declare(
        tl_optional
        GIT_REPOSITORY  https://github.com/TartanLlama/optional.git
        GIT_TAG         c28fcf74d207fc667c4ed3dbae4c251ea551c8c1
)
FetchContent_Declare(
        fetch_tracy
        GIT_REPOSITORY  https://github.com/wolfpld/tracy.git
        GIT_TAG         v0.10
)
FetchContent_Declare(
        fetch_volk
        GIT_REPOSITORY  https://github.com/zeux/volk.git
        GIT_TAG         7121910043955154c585ebb63de2e7d63aefa489
)
FetchContent_Declare(
        vk-bootstrap
        GIT_REPOSITORY  https://github.com/charles-lunarg/vk-bootstrap.git
        GIT_TAG         v1.3.277
)
FetchContent_Declare(
        fetch_vma
        GIT_REPOSITORY  https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator.git
        GIT_TAG         38627f4e37d7a9b13214fd267ec60e0e877e3997
)

FetchContent_MakeAvailable(glm spdlog fetch_fastgltf tl_optional fetch_magic_enum fetch_spirv_reflect
        fetch_tracy fetch_vma vk-bootstrap fetch_volk)

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

find_package(ktx)

# Slang
add_library(slang INTERFACE)
target_include_directories(slang INTERFACE "${CMAKE_CURRENT_LIST_DIR}/slang/include")
target_link_directories(slang INTERFACE "${CMAKE_CURRENT_LIST_DIR}/swlang/lib/windows-x64/release")
target_link_libraries(slang INTERFACE slang)

