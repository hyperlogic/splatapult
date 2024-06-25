Windows Build (vcpkg submodule)
-----------------------
* Install Visual Studio 2022
* Install cmake 3.27.1
* Ensure splatapult has the vcpkg submodule.
  Either clone with the --recursive flag so that the vcpkg submodule is added
  or execute `git submodule init` and `git submodule update` after a regular clone.
* Bootstrap vcpkg
  - `cd vcpkg`
  - `bootstrap-vcpg.bat`
* Execute cmake to create a Visual Studio solution file
  - `mkdir build`
  - `cd build`
  - `cmake .. -G "Visual Studio 17 2022" -DCMAKE_TOOLCHAIN_FILE=vcpkg/scripts/buildsystems/vcpkg.cmake`
* Build exe by using Visual Studio by loading splatapult.sln or from the command line:
  - `cmake --build . --config=Release`

Windows Shipping Builds
-------------------------
The SHIPPING cmake option is used to create a release version of splataplut.
A post build step will copy all of the the data folders (font, shader, texture) into the build directory.
And the resulting exe will use that copy.  You can then zip up the folder and distrubute it to users.

* To create a shipping build:
    - `mkdir build`
    - `cd build`
    - `cmake .. -G "Visual Studio 17 2022" -DCMAKE_TOOLCHAIN_FILE=vcpkg/scripts/buildsystems/vcpkg.cmake -DSHIPPING=ON`
    - `cmake --build . --config=Release`

Linux Build (vcpkg submodule)
--------------------
* Install dependencies
  - `sudo apt-get install clang`
  - `sudo apt-get install cmake`
  - `sudo apt-get install freeglut3-dev`
  - `sudo apt-get install libopenxr-dev`
* Ensure splatapult has the vcpkg submodule.
  Either clone with the --recursive flag so that the vcpkg submodule is added
  or execute `git submodule init` and `git submodule update` after a regular clone.
* Bootstrap vcpkg
  - `cd vcpkg`
  - `bootstrap-vcpg.sh`
* Execute cmake to create a Makefile
  - `mkdir build`
  - `cd build`
  - `cmake .. -G "Unix Makefiles" -DCMAKE_TOOLCHAIN_FILE=vcpkg/scripts/buildsystems/vcpkg.cmake`
* build executable
  - `cmake --build . --config=Release`

*EXPERIMENTAL* Meta Quest Build (OUT OF DATE)
--------------------
NOTE: Although the quest build functions it is much to slow for most scenes.
A Quest2 headset can only run a scene consisting of 25k splats.

* Use vcpkg to install the following packages:
    - `vcpkg install glm:arm64-android`
    - `vcpkg install libpng:arm64-android`
    - `vcpkg install nlohmann-json:arm64-android`
* Set the following environment var ANDROID_VCPKG_DIR to point to vcpkg/installed/arm64-android.
* Download the [Meta OpenXR Mobile SDK 59.0](https://developer.oculus.com/downloads/package/oculus-openxr-mobile-sdk/)
* Install [Android Studio Bumble Bee, Patch 3](https://developer.android.com/studio/archive)
  newer versions do not work with Meta OpenXR Mobile SDK 59.0.
  Follow this guide to setup [Android Studio](https://developer.oculus.com/documentation/native/android/mobile-studio-setup-android/)
* Copy the ovr_openxr_mobile_sdk_59.0 dir into the meta-quest dir.
* Copy the meta-quest/splatapult dir to ovr_openxr_mobile_sdk_59.0/XrSamples/splataplut
* Open the ovr_openxr_mobile_sdk_59.0/XrSamples/splatapult in AndroidStudio.
* Sync and Build

