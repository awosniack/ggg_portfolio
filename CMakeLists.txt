cmake_minimum_required(VERSION 3.15)
project(InventorySystem VERSION 1.0.0 LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Export compile commands for IDE support
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Add compiler warnings
if(MSVC)
    add_compile_options(/W4 /WX-)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# Find threads library (needed for ASIO)
find_package(Threads REQUIRED)

# ASIO standalone (header-only)
# Download and include ASIO if not found
set(ASIO_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/asio/asio/include")
if(NOT EXISTS ${ASIO_INCLUDE_DIR})
    message(STATUS "ASIO not found, will need to be downloaded manually or via script")
endif()

# Add subdirectories
add_subdirectory(shared)
add_subdirectory(server)
add_subdirectory(client)
