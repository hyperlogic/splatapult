Toy program to display 3d gaussian splats
----------------------------------------------

3dgstoy [OPTIONS] DIRECTORY

Options
-------------
-v, --openxr
    launch app in vr mode, using openxr runtime

-f, --fullscreen
    launch window in fullscreen

Runtime options
-------------
* joystick - fly around the scene
* c - toggle between initial colmap point cloud and gaussian splats.
* f - show hide floor carpet.
* n - jump to next camera
* p - jump to previous camera

Directory
-------------
Expects directory structure ready for rendering.

dir/
    point_cloud/
	    iteration_#/
			point_cloud.py
	cfg_args [optional]
	input.ply
	cameras.json [optional]
	vr.json [optional]

By default the iteration_#/point_cloud.py with the highest # is picked.
