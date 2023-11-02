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
* n - jump to next camera
* p - jump to previous camera

Directory
-------------
Expects directory structure ready for rendering.

dir/
    point_cloud/
	    iteration_#/
			point_cloud.py
	cameras.json
	cfg_args
	input.ply

By default the iteration_#/point_cloud.py with the highest # is picked.