cmake_minimum_required(VERSION 3.15)

# Shared libraries
add_library(shared STATIC
    src/Item.cpp
    src/Inventory.cpp
    src/NetworkMessage.cpp
)

target_include_directories(shared PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Link threads for mutex support
target_link_libraries(shared PUBLIC
    Threads::Threads
)

# Add ASIO include directory if available
if(EXISTS ${ASIO_INCLUDE_DIR})
    target_include_directories(shared PUBLIC ${ASIO_INCLUDE_DIR})
    target_compile_definitions(shared PUBLIC ASIO_STANDALONE)
endif()
