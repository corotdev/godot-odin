# 4Players ODIN for Godot (4.4+)

## Description
4Players ODIN is a service to provide in game proximity based voice chat.
While the service does provide Unity, Unreal, C++, etc sdks, a Godot SDK is not provided.

This project is a wrapper around the C++ SDK to provide a Godot interface to the 4Players ODIN service.

## Build
This project is built using SCons. The project uses the Godot C++ bindings to create a GDNative library that can be used in Godot.

### Prerequisites
- Godot 4.4+ (https://godotengine.org/download)
- SCons (https://scons.org/)
- Python 3.12+ (https://www.python.org/downloads/)
- CMake (https://cmake.org/download/)
- prob a bunch of stuff that already comes bundled with macOS

### Build Steps
1. Clone the repository
   ```bash
    git clone --recursive https://github.com/corotdev/godot-odin.git
    cd godot-odin
    ```
2. Run SCons to build the project
   ```bash
   scons target=template_debug macos_arch=universal
   ```
3. TODO (adding to godot)
 