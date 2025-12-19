//
// getnextframe.cpp 
//
// A demonstration of a robust wrapper around PxLGetNextFrame.
//
//

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <vector>
#include <unistd.h>
#include <PixeLINKApi.h>

// Just going to declare a very large buffer here
// One that's large enough for any PixeLINK 4.0 camera
#define MAX_IMAGE_SIZE (5000*5000*2)


#define ASSERT(x)	do { assert((x)); } while(0)

//
// Local Functions
//
static PXL_RETURN_CODE GetNextFrame(HANDLE hCamera, U32 frameBufferSize, void* pFrameBuffer, FRAME_DESC* pFrameDesc, U32 maximumNumberOfTries);



int 
main(const int argc, char const * const argv[])
{
	bool  firstFrame = true;
        float firstFrameTime = 0.0f;	

	// Initialize any camera
	HANDLE hCamera;
	PXL_RETURN_CODE rc = PxLInitializeEx(0, &hCamera, 0);
	if (!API_SUCCESS(rc)) {
		printf("Error: Unable to initialize a camera\n");
		return(1);
	}

	// Just going to declare a very large buffer here
	// One that's large enough for any PixeLINK 4.0 camera
	std::vector<U8> frameBuffer(MAX_IMAGE_SIZE);
	FRAME_DESC frameDesc;
	
	rc = PxLSetStreamState(hCamera, START_STREAM);
	if (API_SUCCESS(rc)) {

		for(int i=0; i < 15; i++) {
			frameDesc.uSize = sizeof(frameDesc);
			rc = GetNextFrame(hCamera, (U32)frameBuffer.size(), &frameBuffer[0], &frameDesc, 5);
			printf("GetNextFrame returned 0x%8.8X\n", rc);
			if (API_SUCCESS(rc)) {
				if (firstFrame) {
					firstFrameTime = frameDesc.fFrameTime;
					firstFrame = false;
				}
				printf("\tframe number %d, frame time %3.3f\n", frameDesc.uFrameNumber, frameDesc.fFrameTime - firstFrameTime);
			} else {
				if (ApiBufferTooSmall == rc) {
					printf ("This program can only capture frames of %d bytes, or smaller.  Reduce the camera's ROI\n", MAX_IMAGE_SIZE);
				}
			}
			printf("\n");
			usleep(500*1000); // 500 milliseconds
		}
	} else {

		printf("PxLSetStreamState failed, rc=0x%X \n", rc);
	}

	rc = PxLSetStreamState(hCamera, STOP_STREAM);
	ASSERT(API_SUCCESS(rc));
	rc = PxLUninitialize(hCamera);
	ASSERT(API_SUCCESS(rc));

	return 0;
}


//
// A robust wrapper around PxLGetNextFrame.
// This will handle the occasional error that can be returned by the API
// because of timeouts. 
//
// Note that this should only be called when grabbing images from 
// a camera NOT currently configured for triggering. 
//
static PXL_RETURN_CODE
GetNextFrame(const HANDLE hCamera, const U32 frameBufferSize, void* const pFrameBuffer, FRAME_DESC* const pFrameDesc, const U32 maximumNumberOfTries)
{
	ASSERT(NULL != pFrameDesc);

	// Record the frame desc size in case we need it later
	const U32 frameDescSize = pFrameDesc->uSize;

	PXL_RETURN_CODE rc = ApiUnknownError;

	for(U32 i=0; i < maximumNumberOfTries; i++) {
		rc = PxLGetNextFrame(hCamera, frameBufferSize, pFrameBuffer, pFrameDesc);
		if (API_SUCCESS(rc)) {
			return rc;
		} else {
			// If the streaming is turned off, or worse yet -- is gone?
			// If so, no sense in continuing.
			if (ApiStreamStopped == rc ||
			    ApiNoCameraAvailableError == rc) {
				return rc;
			} else {
				printf("    Hmmm... PxLGetNextFrame returned 0x%8.8X.\n", rc);
			}
		}
		// Must of just hit a bubble in the frame pipeline.
		// Reset the frame descriptor uSize field (in case the API is newer than what we were compiled with) and try PxLGetNextFrame again.
		pFrameDesc->uSize = frameDescSize;
	}

	// Ran out of tries, so return whatever the last error was.
	return rc;
}
