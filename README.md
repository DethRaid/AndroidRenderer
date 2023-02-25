# Render thing

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
