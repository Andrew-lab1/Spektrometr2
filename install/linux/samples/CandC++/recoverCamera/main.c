//
// main.c
//
// Demonstrates how to 'fully initialize' a camera.  That is, to connect to a camera in an 
// unknown state, and to initialize the camera so that it is in a known, default state.
//
// The techniques shown here are particularly useful if your applications crashes, or is shut 
// down, without having to having done the necessary 'cleanup' operations.
//

#include <stdio.h>
#include <PixeLINKApi.h>

int 
main(int argc, char* argv[])
{
	HANDLE hCamera;

	// Step 1.
	//     Initialize the camera in a stream stopped state.  
	//
	//     If a camera control application 'crashes' while stream a camera, then the 
	//     camera is left in a state where it is trying to output images to the host, 
	//     but the host is not in a state to receive images.  This is characterized 
	//     by the camera's LED flashing red.  Under these circumstances, the USB host 
	//     may have difficulties initializing the camera, and establishing a control
	//     control path to the camera.
	//
	//     Under these circumstances, asserting a stream stop state at initialization, 
	//     will ensure the camera control path gets properly established.  Applications
	//     may want to consider using CAMERA_INITIALIZE_EX_FLAG_ISSUE_STREAM_STOP every 
	//     time they initialize a camera, but keep in mind this may not have the desired
	//     effect if you have multiple applications controlling the same camera.

	// We assume there's only one camera connected;
	if (API_SUCCESS(PxLInitializeEx(0, &hCamera, CAMERA_INITIALIZE_EX_FLAG_ISSUE_STREAM_STOP))) {

		// Step 2.
		//	Assert all camera features to the factory default values.
		//
		//      Rather than leave the camera parameters at their last used value, 
		//      set them to factory default values.

		if (! API_SUCCESS(PxLLoadSettings(hCamera, FACTORY_DEFAULTS_MEMORY_CHANNEL))) {
			printf ("Could not load factory settings!\n");
		} else {
			printf ("Factory default settings restored\n");
		}

		// Uninitialize the camera now that we're done with it.
		PxLUninitialize(hCamera);
	} else {
		printf ("Could not initialize a camera!\n");
	}
	return 0;
}

