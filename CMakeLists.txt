cmake_minimum_required(VERSION 3.0)

project(ndi-test)

# Find includes in corresponding build directories
set(CMAKE_INCLUDE_CURRENT_DIR ON)
# Instruct CMake to run moc automatically when needed.
set(CMAKE_AUTOMOC ON)

# Find the QtWidgets library
find_package(Qt5Widgets)

if (NOT NDI_SDK_PATH)
    find_path(NDI_SDK_PATH include/Processing.NDI.Lib.h
        PATHS ${CMAKE_SOURCE_DIR}/../ndi-sdk)

endif()

if (NOT NDI_SDK_PATH)
    message(FATAL_ERROR "NDI SDK path not found. Please set the NDI_SDK_PATH variable.")
endif()

message(STATUS "Using NDK SDK path: ${NDI_SDK_PATH}")



include_directories("${NDI_SDK_PATH}/include")

link_directories("${NDI_SDK_PATH}/lib/x86_64-linux-gnu")



add_executable(ndi-send "sender/sender.cpp")
target_link_libraries(ndi-send ndi)

add_executable(ndi-view "viewer/viewer.cpp")
target_link_libraries(ndi-view ndi Qt5::Widgets pthread)
