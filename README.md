Splatapult
----------------------------------------------
A program to display 3d gaussian splats files

splatapult [OPTIONS] ply_filename

Options
-------------
-v, --openxr
    launch app in vr mode, using openxr runtime

-f, --fullscreen
    launch window in fullscreen

Desktop Controls
-------------
* joystick - fly around the scene
* c - toggle between initial SfM point cloud (if present) and gaussian splats.
* n - jump to next camera
* p - jump to previous camera
* wasd - move
* arrow keys - look
* right mouse button - hold down for mouse look.

Vr Controls
--------------
* c - toggle between initial SfM point cloud (if present) and gaussian splats.
* left stick - move
* right stick - snap turn
* f - show hide floor carpet.
* single grab - translate the world.
* double grab - rotate and translate the world.
* triple grab - (double grab while trigger is depressed) scale, rotate and translate the world.
* return - save the current position and orientation/scale of the world into a vr.json file.

config files
----------------------
If a "cameras.json" file is found in the same dir as the ply_filename or it's parent dirs, it will be loaded.
In desktop mode the 'n' and 'p' keys can be used to cycle thru the camera viewpoints.

If a "_vr.json" file is found, it will be used to determine the proper starting position, scale and orienation for vr mode.
You can create your own _vr.json file by manipulating the scene via grab in vr mode, then press return to save.

These search paths are setup to support the same dir structure that [Gaussian Splatting](https://github.com/graphdeco-inria/gaussian-splatting) code will output.
input.ply should contain the point cloud and cameras.json will contain the camera orientations from the SfM stage.

```
dir/
    point_cloud/
	    iteration_30000/
			point_cloud.py
			point_cloud_vr.json
	input.ply
	cameras.json
```

It will also support files downloaded from lumalabs.ai, but in this case there will be no point clouds or cameras.

```
dir/
    mycapture.ply
	mycapture_vr.json
```


