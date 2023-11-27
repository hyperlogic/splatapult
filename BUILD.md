Windows Build
-----------------------
* Install Visual Studio 2022
* Install cmake 3.27.1
* Install (vcpkg)[https://github.com/microsoft/vcpkg#quick-start-windows]
* Use vcpkg to install the following packages:
    - `vcpkg install sdl2:x64-windows`
    - `vcpkg install glew:x64-windows`
    - `vcpkg install glm:x64-windows`
    - `vcpkg install libpng:x64-windows`
    - `vcpkg install nlohmann-json:x64-windows`
    - `vcpkg install tracy:x64-windows`
    - `vcpkg install openxr-loader:x64-windows`

* Use cmake to make the visual studio project:
    - `mkdir build`
    - `cd build`
    - `cmake -DCMAKE_TOOLCHAIN_FILE="C:\path\to\vcpkg\scripts\buildsystems\vcpkg.cmake" ..`

Use visual studio to open and build the project.
Or build from the command line: `cmake --build . --config=Release`

Meta Quest
--------------
* Install (Meta OpenXR Mobile SDK 59.0)[https://developer.oculus.com/downloads/package/oculus-openxr-mobile-sdk/]
* Install (Android Studio Bumble Bee, Patch 3)[https://developer.android.com/studio/archive]
  newer versions do not work with Meta OpenXR Mobile SDK 59.0.
  Follow this guide to setup (Android Studio)[https://developer.oculus.com/documentation/native/android/mobile-studio-setup-android/]
* TODO: more to follow
