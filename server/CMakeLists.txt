cmake_minimum_required(VERSION 3.15)

# Server executable
add_executable(server
    src/main.cpp
    src/Server.cpp
    src/ClientSession.cpp
    src/InventoryManager.cpp
    src/SharedStashManager.cpp
    src/ItemRegistry.cpp
)

target_include_directories(server PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(server PRIVATE
    shared
    Threads::Threads
)

# Add ASIO include directory if available
if(EXISTS ${ASIO_INCLUDE_DIR})
    target_include_directories(server PRIVATE ${ASIO_INCLUDE_DIR})
    target_compile_definitions(server PRIVATE ASIO_STANDALONE)
endif()
