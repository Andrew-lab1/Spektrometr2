//
// preview.cpp 
//
// A simple application that previews the camera
//
//

#include <PixeLINKApi.h>
#include <cassert>
#include <stdio.h>
#include "LinuxUtil.h"

#define ASSERT(x)	do { assert((x)); } while(0)

int 
main(int argc, char* argv[])
{
	HANDLE 		hCamera;
	HWND   		previewHandle;
	PXL_RETURN_CODE rc;
    	DontWaitForEnter unbufferedKeyboard;  // Declare this for our kbhit


	// Grab the first camera we find
	rc = PxLInitializeEx(0, &hCamera, 0);
	if (API_SUCCESS(rc)) {

		if (ApiSuccessApiLiteWarning == rc)
	   {
		   printf ("ERROR: Api Lite detected -- this application requires the standard Pixelink API\n");
		   goto Cleanup;
	   }

		// Just use all of the camer's default settings.
		// Start the stream
		rc = PxLSetStreamState(hCamera, START_STREAM);
		ASSERT(API_SUCCESS(rc));

		// Start the preview (NOTE: camera must be streaming)
		rc = PxLSetPreviewState(hCamera, START_PREVIEW, &previewHandle);
		ASSERT(API_SUCCESS(rc));

		// Spin until the user presses a key....
	        printf ("Press any key to exit......\n");
		
		while ( !kbhit() ) {};

		// Stop the preview 
		rc = PxLSetPreviewState(hCamera, STOP_PREVIEW, &previewHandle);
		ASSERT(API_SUCCESS(rc));

		// Stop the stream
		rc = PxLSetStreamState(hCamera, STOP_STREAM);
		ASSERT(API_SUCCESS(rc));

Cleanup:
		// Uninitialize the camera now that we're done with it.
		PxLUninitialize(hCamera);
	}
	return 0;
}



