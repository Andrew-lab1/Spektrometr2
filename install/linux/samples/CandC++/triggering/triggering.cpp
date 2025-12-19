//
// triggering.cpp
//
// A simple example of the use of the two triggering types: 
//  software
//  hardware
// 
// Note 1: This application assumes that a camera that is capable of
// hardware triggering (IE, it supports hardware triggering), is capable
// of receiving a hardware trigger (IE, it has a 'hardware trigger jig'
// connected).
//
// Note 2: We do NOT explore here the triggering modes such as 
// Mode 0 vs Mode 14 et cetera. The PixeLINK documentation provides 
// details about the various triggering modes.
//
// Note 3: We assume that there's only one PixeLINK camera connected to 
// the computer. 
//
#include <PixeLINKApi.h>
#include "LinuxUtil.h"
#include <stdio.h>
#include <unistd.h>
#include <stdio.h>
#include <assert.h>
#include <vector>


// Used as a simple sanity check in this example file
#define ASSERT(x)	do { assert((x)); } while (0)

// Local functions
static bool IsTriggeringSupported(HANDLE hCamera);
static void SetTriggering(HANDLE hCamera, int mode, int triggerType, int polarity, float delay, float param);
static void DisableTriggering(HANDLE hCamera);
static void TestSoftwareTrigger(HANDLE hCamera);
static void TestHardwareTrigger(HANDLE hCamera);


int 
main(int argc, char* argv[])
{
	// Initialize any camera
	HANDLE hCamera;
	PXL_RETURN_CODE rc = PxLInitialize(0, &hCamera);
	if (!API_SUCCESS(rc)) {
		printf("ERROR Unable to initialize a camera\n");
		return EXIT_FAILURE;
	}

	// If the camera doesn't support triggering, we're done.
	if (!IsTriggeringSupported(hCamera)) {
		printf("Triggering is not supported on this camera\n");
		PxLUninitialize(hCamera);
		return EXIT_FAILURE;
	}

	// Start with triggering disabled so we start with a clean slate
	DisableTriggering(hCamera);


	// Test the three major types of triggering
	// We only use Mode 0 triggering.
	//TestSoftwareTrigger(hCamera);
	TestHardwareTrigger(hCamera);

	// Put the camera back to a known state
	DisableTriggering(hCamera);

	PxLUninitialize(hCamera);
	return EXIT_SUCCESS;
}

//
// Not all PixeLINK cameras support triggering.
// Machine vision cameras support triggering, but 
// microscopy cameras do not.
//
//
static bool
IsTriggeringSupported(HANDLE hCamera)
{
	// How big a buffer will we need to hold the information about the trigger feature?
	U32 bufferSize = -1;
	PXL_RETURN_CODE rc = PxLGetCameraFeatures(hCamera, FEATURE_TRIGGER, NULL, &bufferSize);
	ASSERT(API_SUCCESS(rc));
	ASSERT(bufferSize > 0);

	// Declare a buffer and read the feature information
	std::vector<U8> buffer(bufferSize, 0);  // zero-initialized buffer
	CAMERA_FEATURES* pCameraFeatures = (CAMERA_FEATURES*)&buffer[0];
	rc = PxLGetCameraFeatures(hCamera, FEATURE_TRIGGER, pCameraFeatures, &bufferSize);
	ASSERT(API_SUCCESS(rc));

	// Check the sanity of the return information
	ASSERT(1 == pCameraFeatures->uNumberOfFeatures);			// We only asked about one feature...
	ASSERT(FEATURE_TRIGGER == pCameraFeatures->pFeatures[0].uFeatureId);	// ... and that feature is triggering

	bool isSupported = ((pCameraFeatures[0].pFeatures->uFlags & FEATURE_FLAG_PRESENCE) != 0);
	if (isSupported) {
		// While we're here, check an assumption about the number of parameters
		ASSERT(FEATURE_TRIGGER_NUM_PARAMS == pCameraFeatures->pFeatures[0].uNumberOfParameters);
	}

	return isSupported;
}



//
// Set up the camera for triggering, and, enable triggering.
//
static void
SetTriggering(HANDLE hCamera, int mode, int triggerType, int polarity, float delay, float param)
{
	U32 flags;
	U32 numParams = FEATURE_TRIGGER_NUM_PARAMS;
	float params[FEATURE_TRIGGER_NUM_PARAMS];

	// Read current settings
	PXL_RETURN_CODE rc = PxLGetFeature(hCamera, FEATURE_TRIGGER, &flags, &numParams, &params[0]);
	ASSERT(API_SUCCESS(rc));
	ASSERT(5 == numParams);

	// Very important step: Enable triggering by clearing the FEATURE_FLAG_OFF bit
	flags = ENABLE_FEATURE(flags, true);

	// Assign the new values...
	params[FEATURE_TRIGGER_PARAM_MODE]	= (float)mode;
	params[FEATURE_TRIGGER_PARAM_TYPE]	= (float)triggerType;
	params[FEATURE_TRIGGER_PARAM_POLARITY]	= (float)polarity;
	params[FEATURE_TRIGGER_PARAM_DELAY]	= delay;
	params[FEATURE_TRIGGER_PARAM_PARAMETER]	= param;

	// ... and write them to the camera
	rc = PxLSetFeature(hCamera, FEATURE_TRIGGER, flags, numParams, &params[0]);
	ASSERT(API_SUCCESS(rc));
}

static void
DisableTriggering(HANDLE hCamera)
{
	U32 flags;
	U32 numParams = 5;
	float params[5];

	// Read current settings
	PXL_RETURN_CODE rc = PxLGetFeature(hCamera, FEATURE_TRIGGER, &flags, &numParams, &params[0]);
	ASSERT(API_SUCCESS(rc));
	ASSERT(5 == numParams);

	// Disable triggering
	flags = ENABLE_FEATURE(flags, false);

	rc = PxLSetFeature(hCamera, FEATURE_TRIGGER, flags, numParams, &params[0]);
	ASSERT(API_SUCCESS(rc));
}

//
// Quick and dirty routine to capture an image (and do nothing with it)
//
static void 
CaptureImage(HANDLE hCamera)
{
	// Large buffer that will handle all currently supported cameras in 16-bit mode
	std::vector<U8> frame(3000*3000*sizeof(U16)); 

	// Very important: tell the API what version of FRAME_DESC we're using
	FRAME_DESC frameDesc;
	frameDesc.uSize = sizeof(frameDesc); 
	
	PXL_RETURN_CODE rc = PxLGetNextFrame(hCamera, (U32)frame.size(), &frame[0], &frameDesc);
	ASSERT(API_SUCCESS(rc));
	printf("Image captured.\n");
}

//
// With software triggering, calling PxLGetNextFrame causes 
// the camera to capture an image. The camera must be in the 
// streaming state, but no image will be 'streamed' to the host
// until PxLGetNextFrame is called.
//
//
static void 
TestSoftwareTrigger(HANDLE hCamera)
{
	printf("\nConfiguring the camera for software triggering\n");
	SetTriggering(hCamera, 
		0,				// Mode 0 Triggering
		TRIGGER_TYPE_SOFTWARE, 
		POLARITY_ACTIVE_LOW, 
		0.0,				// no delay
		0);				// unused for Mode 0

	PXL_RETURN_CODE rc = PxLSetStreamState(hCamera, START_STREAM);
	ASSERT(API_SUCCESS(rc));

	// We can now grab two images (without blocking)
	printf("Capturing two images...\n");
	CaptureImage(hCamera);
	CaptureImage(hCamera);
	printf("done.\n");

	rc = PxLSetStreamState(hCamera, STOP_STREAM);
	ASSERT(API_SUCCESS(rc));
}

//
// With hardware triggering, the camera doesn't take an image until the 
// trigger input of the machine vision connector is activated. 
//
//
static void 
TestHardwareTrigger(HANDLE hCamera)
{
	printf("\nConfiguring the camera for hardware triggering...\n");
	SetTriggering(hCamera, 
		0,							// Mode 0 Triggering
		TRIGGER_TYPE_HARDWARE, 
		POLARITY_ACTIVE_LOW, 
		0.0,							// no delay
		0);							// unused for Mode 0

	PXL_RETURN_CODE rc = PxLSetStreamState(hCamera, START_STREAM);
	ASSERT(API_SUCCESS(rc));

	printf("Waiting for a hardware trigger...\n");
	CaptureImage(hCamera);
	printf("Waiting for one more hardware trigger...\n");
	CaptureImage(hCamera);

	rc = PxLSetStreamState(hCamera, STOP_STREAM);
	ASSERT(API_SUCCESS(rc));
}
