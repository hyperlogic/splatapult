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
* Use vcpkg to install the following packages:
    - `vcpkg install glm:arm64-android`
    - `vcpkg install libpng:arm64-android`
    - `vcpkg install nlohmann-json:arm64-android`
* Set the following environment var ANDROID_VCPKG_DIR to point to vcpkg/installed/arm64-android.
* Download the (Meta OpenXR Mobile SDK 59.0)[https://developer.oculus.com/downloads/package/oculus-openxr-mobile-sdk/]
* Install (Android Studio Bumble Bee, Patch 3)[https://developer.android.com/studio/archive]
  newer versions do not work with Meta OpenXR Mobile SDK 59.0.
  Follow this guide to setup (Android Studio)[https://developer.oculus.com/documentation/native/android/mobile-studio-setup-android/]
* Copy the ovr_openxr_mobile_sdk_59.0 dir into the meta-quest dir.
* Copy the meta-quest/splatapult dir to ovr_openxr_mobile_sdk_59.0/XrSamples/splataplut
* Open the ovr_openxr_mobile_sdk_59.0/XrSamples/splatapult in AndroidStudio.
* Sync and Build
