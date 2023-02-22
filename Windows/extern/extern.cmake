include(FetchContent)

set(BUILD_TESTING OFF CACHE BOOL "" FORCE)

set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE) 
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
        fetch_glfw
        GIT_REPOSITORY  https://github.com/glfw/glfw.git
        GIT_TAG         3.3.8
)
FetchContent_MakeAvailable(fetch_glfw)
