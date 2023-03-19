# Render thing

## Build Prerequisites

- Install [libKTX](https://github.com/KhronosGroup/KTX-Software). Find the latest release in the Releases tab and install it to `D:/Program Files/KTX-Software/lib/cmake/ktx`. If your system does not have a D drive, you'll have to modify `RenderCore/extern/extern.cmake` to set `KTX_DIR` to the directory you installed libKTX to
- This could be avoided if `libKTX` didn't ahve a hard dependency on Bash... which isn't installed on Windows....

## Android build

- Symlink or copy the folder `RenderCore/Shaders/` to `app/src/main/shaders/`
- Copy the glTF Sponza to `app/src/main/assets/Sponza/`
- Build the Gradle project in `app/`
- Run the APK

## Windows build

- Build the cmake project in `Windows`
- Symlink (or copy) the folder `RenderCore/shaders/` to `Windows/build/Debug/shaders/` (or wherever the working directory for your build is)
- Copy the glTF Sponza to `Windows/build/Debug/Sponza/`
- Run the SahWindows executable

## Other OSs

Unsupported. You could probably modify the Windows build to also run on Linux, but I've had no reason to
