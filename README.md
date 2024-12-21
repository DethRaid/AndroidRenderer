# Render thing

## Build Prerequisites

- Install [libKTX](https://github.com/KhronosGroup/KTX-Software). Find the latest release in the Releases tab and install it to `D:/Program Files/KTX-Software`. If your system does not have a D drive, you'll have to modify `RenderCore/extern/extern.cmake` to set `KTX_DIR` to the directory you installed libKTX to
- Optinally, install [Nvidia's Optical Flow SDK](https://developer.nvidia.com/optical-flow-sdk) to `RenderCore/extern/Optical_Flow_SDK_5.0.7`
- You'll also need to install the Vulkan SDK and add its `bin` directory to your PATH
- Download "the glTF Sponza" from https://github.com/KhronosGroup/glTF-Sample-Models/tree/master/2.0/Sponza. You'll need to manually copy it to your working directory before running

## Android build

- Symlink or copy the folder `RenderCore/Shaders/` to `app/src/main/shaders/`
- Copy the glTF Sponza to `app/src/main/assets/Sponza/`
- Build the Gradle project in `app/`
- Run the APK

## Windows build
- Download `gltfpack` from https://github.com/zeux/meshoptimizer and place it in your PATH
- Copy the [glTF Sponza](https://github.com/KhronosGroup/glTF-Sample-Models/tree/master/2.0/Sponza) to `Windows/build/Sponza/`
- Symlink (or copy) the folder `RenderCore/shaders/` to `Windows/build/shaders/`
- Build the cmake project in `Windows`
- Run the SahWindows Visual Studio project
- If you want to run from outside Visual Studio, either copy Sponza and the shaders to `Windows/build/Debug` or copy the SahWindows executable to `Windows/build`

## Other OSs

Unsupported. You could probably modify the Windows build to also run on Linux, but I've had no reason to

## Known-good configurations

- This program definitely works on a Nvidia RTX 2080 Super GPU, and a Mali G710 GPU. Other Vulkan implementations may or may not support all the required features

## Acknowledgements

This software contains source code provided by NVIDIA Corporation.
