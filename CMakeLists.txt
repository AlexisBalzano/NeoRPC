cmake_minimum_required(VERSION 3.14)
set(CMAKE_TOOLCHAIN_FILE ${CMAKE_CURRENT_SOURCE_DIR}/vcpkg/scripts/buildsystems/vcpkg.cmake)
project(NeoRPC VERSION "1.4.0")

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

if (CMAKE_SYSTEM_NAME STREQUAL "Windows")
    add_compile_definitions(DIR_SEPARATOR="\\\\")
else()
    add_compile_definitions(DIR_SEPARATOR="/")
endif()

if(DEV)
    message(STATUS "Development mode enabled")
    add_compile_definitions(DEV=1)
endif()


configure_file(
    ${CMAKE_SOURCE_DIR}/src/Version.h.in
    ${CMAKE_BINARY_DIR}/Version.h
)

# set DEBUG mode
if (DEBUG)
    add_compile_definitions(
        DEBUG=1
    )
    message(STATUS "DEBUG mode enabled")
endif()

add_subdirectory(External/NeoRadarSDK)

# Add SDK include directory
include_directories(
    ${CMAKE_BINARY_DIR}
    ${CMAKE_SOURCE_DIR}/src
)

# Find external dependencies
add_subdirectory(External/discord-presence)

# Source files
# To set after starting development
set(SOURCES
    src/NeoRPC.cpp
    src/main.cpp
)

# Define the plugin library
add_library(${PROJECT_NAME} SHARED ${SOURCES})

target_link_libraries(${PROJECT_NAME} PRIVATE
    NeoRadarSDK::NeoRadarSDK
    discord-rpc
)

# Set output directory and properties
set_target_properties(${PROJECT_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
    PREFIX ""  # Remove "lib" prefix on Unix-like systems
)

# Add processor-specific output name for Apple platforms
if(${CMAKE_HOST_APPLE})
    set_target_properties(${PROJECT_NAME} PROPERTIES
        OUTPUT_NAME ${PROJECT_NAME}-${CMAKE_HOST_SYSTEM_PROCESSOR}
    )
endif()
