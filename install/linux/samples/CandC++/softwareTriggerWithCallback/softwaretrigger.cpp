//
// softwaretrigger.cpp
//
// A demonstration of a special software triggering mode.
// 
// With normal software triggering on PixeLINK cameras, a call to PxLGetNextFrame will cause the API to:
//
// 1) notify the camera to take an image
// 2) wait until the image is returned.
// 3) return the image 
//
// If the camera were configured to have a 10 second exposure, the thread calling PxLGetNextFrame would be blocked
// inside PxLGetNextFrame for the entire 10 seconds. 
//
// By using callbacks and calling PxLGetNextFrame in a special way, it's possible to emulate a hardware trigger. 
// Calling PxLGetNextFrame with a NULL pFrame pointer and non-zero buffer size will:
//
// 1) notify the camera to take an image. 
//
// and that's it. How do you get the image? It will be handed to you in an CALLBACK_FRAME callback. 
//
//
// Using this technique, it's possible to sofware trigger multiple cameras reasonably simultaneously.
// This technique is demonstrated here. 
//
// This application will detect and connect to all PixeLINK 4.0 cameras, configure each for software triggering
// (assuming all cameras support triggering), set them all streaming, and then software trigger them at 
// regular intervals.
//
//

#include <PixeLINKApi.h>
#include "LinuxUtil.h"
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/syscall.h>
#include <vector>
#include <cassert>
#include <string.h>

//
// A few useful defines and enums.
//
#define ASSERT(x)	do { assert((x)); } while(0)

//
// Local functions
//
static PXL_RETURN_CODE	PrepareCameraForTest(HANDLE hCamera);
static PXL_RETURN_CODE	SetUpSoftwareTriggering(HANDLE hCamera);
static void		DoTest(const std::vector<HANDLE>& hCameras, U32 numberOfCapturesToDo);
static U32		ImageCallbackFunction(HANDLE hCamera, void* pFrameData, U32 dataFormat, FRAME_DESC const * pFrameDesc, void* context);




int 
main(const int argc, char const * const argv[])
{
	int returnCode = EXIT_FAILURE;

	// First determine how many cameras there are
	U32 numberOfCameras = 0;
	PXL_RETURN_CODE rc = PxLGetNumberCameras(NULL, &numberOfCameras);
	if (!API_SUCCESS(rc)) {
		printf("Error: Unable to determine the number of cameras\n");
		return EXIT_FAILURE;
	}

	if (0 == numberOfCameras) {
		printf("Error: No cameras connected\n");
		return EXIT_FAILURE;
	}

	// Now get a list of serial numbers
	std::vector<U32 > serialNumbers(numberOfCameras,0);
	rc = PxLGetNumberCameras(&serialNumbers[0], &numberOfCameras);
	if (!API_SUCCESS(rc)) {
		printf("Error: Unable to read the camera serial numbers\n");
		return EXIT_FAILURE;
	}
	ASSERT(numberOfCameras == serialNumbers.size());

	// Initialize each of the cameras
	std::vector<HANDLE> hCameras;
	for(size_t i=0; i < serialNumbers.size(); i++) {
		HANDLE hCamera;
		if (!API_SUCCESS(PxLInitialize(serialNumbers[i], &hCamera))) {
			printf("Error: Unable to initialize camera %d\n", serialNumbers[i]);
		}
		if (API_SUCCESS(PrepareCameraForTest(hCamera))) {
			printf("Camera %d has handle 0x%p\n", serialNumbers[i], hCamera);
			hCameras.push_back(hCamera);
		} else {
			printf("Error: Unable to prepare camera %d\n", serialNumbers[i]);
			PxLUninitialize(hCamera);
			break;
		}
	}
	
	// Did we initialize all of them successfully?
	if (hCameras.size() == numberOfCameras) {
		const U32 numberOfCapturesToDo = 5;
		DoTest(hCameras, numberOfCapturesToDo);
		returnCode = EXIT_SUCCESS;
	}

	for(size_t i = 0; i < hCameras.size(); i++) {
		PxLSetCallback(hCameras[i], CALLBACK_FRAME, 0, NULL);
		PxLSetStreamState(hCameras[i], STOP_STREAM);
		PxLUninitialize(hCameras[i]);
	}

	return returnCode;
}

//
// Given a camera handle, configure the camera for software triggering. 
//
static PXL_RETURN_CODE
SetUpSoftwareTriggering(const HANDLE hCamera)
{
	U32 flags;
	U32 numParams;

	PXL_RETURN_CODE rc = PxLGetFeature(hCamera, FEATURE_TRIGGER, &flags, &numParams, NULL);
	if (API_SUCCESS(rc)) {
		ASSERT(numParams >= FEATURE_TRIGGER_NUM_PARAMS);

		std::vector<float> params(numParams,0.0f);
		rc = PxLGetFeature(hCamera, FEATURE_TRIGGER, &flags, &numParams, &params[0]);
		if (API_SUCCESS(rc)) {

			params[FEATURE_TRIGGER_PARAM_MODE] = 0;
			params[FEATURE_TRIGGER_PARAM_TYPE] = TRIGGER_TYPE_SOFTWARE;
			// Leave the rest of the params the way they are

			flags = ENABLE_FEATURE(flags, true);	// enable triggering
			rc = PxLSetFeature(hCamera, FEATURE_TRIGGER, flags, numParams, &params[0]);
		}
	}
	return rc;
}



static PXL_RETURN_CODE
PrepareCameraForTest(const HANDLE hCamera)
{
	PXL_RETURN_CODE rc = SetUpSoftwareTriggering(hCamera);
	if (API_SUCCESS(rc)) {
		// Set up CALLBACK_FRAME callbacks for this camera
		rc = PxLSetCallback(hCamera, CALLBACK_FRAME, NULL, ImageCallbackFunction);
		if (API_SUCCESS(rc)) {
			return PxLSetStreamState(hCamera, START_STREAM);
		}
	}
	return rc;

}

//
// This CALLBACK_FRAME callback function will be called when an image is available from the camera.
//
// Note that frame time is relative to when the camera started streaming, so you have to make sure that 
// the cameras start streaming at approximately the same time. If you're single-stepping through this code 
// in a debugger, that's likely not the case. 
//
static U32
ImageCallbackFunction(const HANDLE hCamera, void* const pFrameData, const U32 dataFormat, FRAME_DESC const * const pFrameDesc, void* const context)
{
	printf("Callback on thread 0x%ld for camera 0x%p, frameTime = %f\n", syscall(SYS_gettid), hCamera, pFrameDesc->fFrameTime);
	return ApiSuccess;
}


static void 
DoTest(const std::vector<HANDLE>& hCameras, const U32 numberOfCapturesToDo)
{
	// Cameras should be streaming now and ready.
	
	for(U32 numTimes = 0; numTimes < numberOfCapturesToDo; numTimes++) {
		sleep(1); // Wait a sec...
		printf("\n");

		// Fire a software trigger on all of the cameras as quickly as possible
		const U32 numCameras = hCameras.size();
		const U32 bufferSize = 1; // We just have to provide a non-zero buffer size.
		for(size_t i =0; i < numCameras; i++) {
			// Initialize a frame descriptor
			FRAME_DESC frameDesc;
			memset(&frameDesc, 0, sizeof(FRAME_DESC));	// Not necessary, but we're trying to show that the frame descriptor doesn't get changed.
			ASSERT(0.0f == frameDesc.Shutter.fValue);
			frameDesc.uSize = sizeof(FRAME_DESC);		// This IS necessary.

			// By passing in a frame pointer of NULL, we're saying to a camera that is configured for software triggering 
			// that we just want to quickly trigger the camera, and not wait for the image to be exposed and sent back to the host.
			// Just start taking the picture. 
			// We'll receive the picture in the callback function we've registered.
			PXL_RETURN_CODE rc = PxLGetNextFrame(hCameras[i], bufferSize, NULL, &frameDesc);
			if (!API_SUCCESS(rc)) {
				printf("Error: PxLGetNextFrame returned 0x%8.8X\n", rc);
			} else {
				printf("Software trigger sent to 0x%p\n", hCameras[i]);
			}

			// Note that the frame descriptor has not been modified.
			ASSERT(0.0f == frameDesc.Shutter.fValue);
		}
	}
}


