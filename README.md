# Inventory System - C++ Portfolio Project

A C++ client-server inventory management system.

## Demo Video

[![Watch Demo Video](https://img.youtube.com/vi/IpXiM5fGRxU/maxresdefault.jpg)](https://www.youtube.com/watch?v=IpXiM5fGRxU)

## Features

- **Client-Server Architecture**: Server authoritative design where all game logic runs on the server
- **Personal Inventory**: Each client has a 5x12 grid inventory
- **Shared Stashes**: 3 shared 12x12 stashes with race condition handling
- **Variable Item Sizes**: Items can range from 1x1 to 2x4 grid cells
- **Stack System**: Items support stacking.
- **Graphical Interface**: Built with raylib.

## Project Structure

```
├── client/          # Client application (rendering + input)
├── server/          # Server application (logic + networking)
├── shared/          # Shared code (data models, protocols)
└── CMakeLists.txt   # Root build configuration
```

## Building

### Prerequisites

- CMake 3.15 or higher
- C++17 compatible compiler (GCC 7+, Clang 5+, MSVC 2017+)
- Git (for fetching dependencies)

### Download ASIO (for networking)

```bash
mkdir -p third_party
cd third_party
git clone https://github.com/chriskohlhoff/asio.git
cd ..
```

### Build Instructions

```bash
# Create build directory
mkdir build
cd build

# Configure
cmake ..

# Build
cmake --build .

# Run server
./server/server

# Run client (in another terminal)
./client/client
```

## Usage

1. Start the server first
2. Launch one or more clients
3. Clients will display their personal inventory and the three shared stashes
4. Drag and drop items between inventories
5. Server handles all item movements


## Author

André Wosniack - 02/2026

## License

This project is for portfolio purposes.
