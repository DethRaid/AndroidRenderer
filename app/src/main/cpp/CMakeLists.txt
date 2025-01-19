#[[
Copyright 2022 Google Inc. All Rights Reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

     http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
]]
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

# Dependencies

# Add a static library called “playcore” built with the c++_static STL.
include(${PLAYCORE_LOCATION}/playcore.cmake)
add_playcore_static_library()

include(${CMAKE_CURRENT_LIST_DIR}/../../../../RenderCore/SahCore.cmake)

# Build

# Integrate GameActivity, refer to
#     https://d.android.com/games/agdk/integrate-game-activity
# for the detailed instructions.
find_package(game-activity REQUIRED CONFIG)

file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS *.cpp *.hpp)

add_library(${CMAKE_PROJECT_NAME} SHARED ${SOURCES})

target_compile_definitions(${CMAKE_PROJECT_NAME}
        PUBLIC
        VK_USE_PLATFORM_ANDROID_KHR
        )

target_include_directories(${CMAKE_PROJECT_NAME} PUBLIC
        ${CMAKE_SOURCE_DIR}
        ${PLAYCORE_LOCATION}/include
        )

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wno-format-security")
target_link_libraries(${CMAKE_PROJECT_NAME}
        game-activity::game-activity
        log
        android
        playcore

        SahCore
        )

set(SHADER_INCLUDE_DIR ${CMAKE_CURRENT_LIST_DIR}/../RenderCore)

message(STATUS "CMAKE_CURRENT_LIST_DIR=${CMAKE_CURRENT_LIST_DIR}")

add_custom_target(compile_shaders
        python ${SAH_TOOLS_DIR}/compile_shaders.py ${SHADER_DIR} ${CMAKE_BINARY_DIR}/shaders
)
add_dependencies(${CMAKE_PROJECT_NAME} compile_shaders)
