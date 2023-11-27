Splatapult
----------------------------------------------
A program to display 3d gaussian splats files

splatapult [OPTIONS] DIRECTORY

Options
-------------
-v, --openxr
    launch app in vr mode, using openxr runtime

-f, --fullscreen
    launch window in fullscreen

Desktop Controls
-------------
* joystick - fly around the scene
* c - toggle between initial colmap point cloud and gaussian splats.
* n - jump to next camera
* p - jump to previous camera
* wasd - move
* arrow keys - look

Vr Controls
--------------
* c - toggle between initial colmap point cloud and gaussian splats.
* left stick - move
* right stick - snap turn
* f - show hide floor carpet.
* single grab - translate the world.
* double grab - rotate and translate the world.
* triple grab - (double grab while trigger is depressed) scale, rotate and translate the world.
* return - save the current position and orientation/scale of the world into a vr.json file.

Directory
-------------
Expects directory structure ready for rendering.

dir/
    point_cloud/
	    iteration_#/
			point_cloud.py
	cfg_args [optional]
	input.ply [optional]
	cameras.json [optional]
	vr.json [optional]

By default the iteration_#/point_cloud.py with the highest # is picked.
