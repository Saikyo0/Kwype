cmake_minimum_required(VERSION 3.17)
project(program)

# ---------------------- CONFIG

if(NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Debug CACHE STRING "Choose the type of build." FORCE)
endif()

set(CMAKE_CONFIGURATION_TYPES "Debug;Release" CACHE STRING "" FORCE)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)
set_property(GLOBAL PROPERTY PREDEFINED_TARGETS_FOLDER "CMakeTargets")

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/Debug")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/Release")

# ---------------------- PROGRAM

# -- ADD PROGRAM
add_executable(program main.cpp)
set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT program)

# ---------------------- INCLUDE

# -- JSON INCLUDE
target_include_directories(program PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/nlohmann")

# -- TFLITE INCLUDE
target_include_directories(program PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/tflite-dist/include")

# -- TFLITE FLATBUFFERS LIB
find_library(TFLITE_LIB tensorflowlite.dll.if HINTS "${CMAKE_CURRENT_SOURCE_DIR}/tflite-dist/libs/windows_x64")
find_library(FLATBUFFERS_LIB flatbuffers HINTS "${CMAKE_CURRENT_SOURCE_DIR}/tflite-dist/libs/windows_x64")
if(NOT TFLITE_LIB OR NOT FLATBUFFERS_LIB)
    message(FATAL_ERROR "Could not find lib files in tflite-dist/libs/windows_x64")
endif()

# -- RAYLIB INCLUDE
target_include_directories(program PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/raylib/include")

# -- RAYLIB LIB
find_library(RAYLIB_LIB raylib HINTS "${CMAKE_CURRENT_SOURCE_DIR}/raylib/lib")
if(NOT RAYLIB_LIB)
    message(FATAL_ERROR "Could not find lib files in raylib/lib")
endif()

# ---------------------- LINKER

# -- LINK JSON LIB
target_link_libraries(program PRIVATE ${JSON_LIB})
    
# -- LINK TFLITE FLATBUFFERS LIB
target_link_libraries(program PRIVATE ${TFLITE_LIB} ${FLATBUFFERS_LIB})

# -- LINK TFLITE FLATBUFFERS DLL
add_custom_command(
    TARGET program POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/tflite-dist/libs/windows_x64/tensorflowlite.dll"
        "$<TARGET_FILE_DIR:program>"
    COMMENT "Deploying tflite.dll to build folder..."
)

# -- LINK RAYLIB LIB
target_link_libraries(program PRIVATE ${RAYLIB_LIB} "winmm.lib"
    "gdi32.lib"
    "opengl32.lib"
    "shell32.lib"
    "user32.lib")

# -- LINK RAYLIB DLL
add_custom_command(
    TARGET program POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/raylib/lib/raylib.dll"
        "$<TARGET_FILE_DIR:program>"
    COMMENT "Deploying raylib.dll to build folder..."
)