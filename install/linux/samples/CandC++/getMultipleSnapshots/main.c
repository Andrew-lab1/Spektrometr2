//
// main.c
//
// Sample code to capture an image from a PixeLINK camera and save 
// the encoded image to a file.
//

#include <stdio.h>
#include <unistd.h>
#include <PixeLINKApi.h>
#include "getsnapshot.h"

int 
main(int argc, char* argv[])
{
	HANDLE hCamera;
	char pFilenameBmp[40];
	int retVal;
        int i;

	// Tell the camera we want to start using it.
	// NOTE: We're assuming there's only one camera.
	if (!API_SUCCESS(PxLInitialize(0, &hCamera))) {
		return 1;
	}

	// Put camera into streaming state so we can capture an image
	if (!API_SUCCESS(PxLSetStreamState(hCamera, START_STREAM))) {
		return 1;
	}


	// Get the snapshots and save it to a file
        for (i=0; i<5; i++)
        {
           sprintf (pFilenameBmp, "image%d.jpg", i);
           retVal = GetSnapshot(hCamera, IMAGE_FORMAT_JPEG, pFilenameBmp);
	   if (SUCCESS == retVal) {
		printf("Saved image to '%s'\n", pFilenameBmp);
           } else {                
		printf("Oh-oh, Could not save image to '%s'\n", pFilenameBmp);
           }
           sleep(2);
        }


	// Put camera into streaming state so we can capture an image
	if (!API_SUCCESS(PxLSetStreamState(hCamera, STOP_STREAM))) {
		return 1;
	}

	// Tell the camera we're done with it.
	PxLUninitialize(hCamera);

	return (SUCCESS == retVal) ? 0 : 1;
}
