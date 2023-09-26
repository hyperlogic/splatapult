Toy program to display 3d gaussian splats
----------------------------------------------

Windows Build
-----------------------
Install cmake 3.27.1
Install vcpkg[https://github.com/microsoft/vcpkg#quick-start-windows]
Use vcpkg to install the following packages:
* `vcpkg install sdl2:x64-windows`
* `vcpkg install glew:x64-windows`
* `vcpkg install glm:x64-windows`
* `vcpkg install libpng:x64-windows`

Use cmake to make the visual studio project:
* `mkdir build`
* `cd build`
* `cmake -DCMAKE_TOOLCHAIN_FILE="C:\msys64\home\ajthy\code2\vcpkg\scripts\buildsystems\vcpkg.cmake" ..`

Use visual studio to open and build the project

To build from the command line:
* `cmake --build . --config=Release`




