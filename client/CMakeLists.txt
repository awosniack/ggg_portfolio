cmake_minimum_required(VERSION 3.15)

# Try to find raylib
find_package(raylib QUIET)

if(NOT raylib_FOUND)
    # Fetch raylib from GitHub if not found
    include(FetchContent)
    FetchContent_Declare(
        raylib
        GIT_REPOSITORY https://github.com/raysan5/raylib.git
        GIT_TAG 5.0
    )
    FetchContent_MakeAvailable(raylib)
endif()

# Client executable
add_executable(client
    src/main.cpp
    src/Client.cpp
    src/ClientInventory.cpp
    src/Renderer.cpp
    src/InputHandler.cpp
    src/UIManager.cpp
    src/ItemIcons.cpp
)

target_include_directories(client PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(client PRIVATE
    shared
    raylib
    Threads::Threads
)

# Add ASIO include directory if available
if(EXISTS ${ASIO_INCLUDE_DIR})
    target_include_directories(client PRIVATE ${ASIO_INCLUDE_DIR})
    target_compile_definitions(client PRIVATE ASIO_STANDALONE)
endif()

# Copy assets to build directory
file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/assets DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
