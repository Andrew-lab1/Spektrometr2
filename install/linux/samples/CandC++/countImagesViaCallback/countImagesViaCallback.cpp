//
// This demonstrates how use a frame callback, to receive images quickly.  It will report
// on the number of images reviced, as well as the number of lost images.
//
//

#include "PixeLINKApi.h"
#include "LinuxUtil.h"

#include <iostream>
#include <stdio.h>
#include <stdexcept>
#include <unistd.h>
#include <time.h>
#include <pthread.h>

using namespace std;

#define A_OK          0  // non-zero error codes
#define GENERAL_ERROR 1

#define BILLION 1E9

//
// Define some globals shared between the main thread, and the callbacks
U32    gFrameCount = 0;     // Total number of frames received
int    gLostFrameCount = 0; // Number of Frames lost.  Note that a negative
                           //    value indicates a frame was dupliced, something
                           //    that should not happen

U32    gExpectedFrameNum;   // Only valid after we receive our first frame
bool   gReceivedFirstFrame = FALSE;

pthread_mutex_t gMutex;

static U32 FrameCallbackFunction(
	HANDLE		hCamera,
	LPVOID		pData,
	U32			dataFormat,
	FRAME_DESC const *	pFrameDesc,
	LPVOID		userData)
{
    U32  frameNumDelta;

    pthread_mutex_lock(&gMutex);
    
    if (!gReceivedFirstFrame)
    {
        gReceivedFirstFrame = TRUE;
        gExpectedFrameNum = pFrameDesc->uFrameNumber;
    }
    gFrameCount++;

    if (pFrameDesc->uFrameNumber < gExpectedFrameNum)
    {
        // This frame is 'older' than expected.  We are just receiving this one
        // later than exected, because the Windows OS happeneded to run a new frame
        // before this one.  This frame wsa previouly reported as lost, so decrease
        // the lost count by 1
        gLostFrameCount--;
    } else {
        // This frame is either the one expected, or one newer than expected.  In this
        // latter case, we will assume that all of the missing frames are indeed lost, and
        // count them as a lost frame.  However if we are wrong and the lost frame is to
        // be delivered later, than gLostFrameCount will be corrected then
        frameNumDelta = pFrameDesc->uFrameNumber - gExpectedFrameNum;
        gLostFrameCount += frameNumDelta;
        gExpectedFrameNum = pFrameDesc->uFrameNumber + 1;
    }

    pthread_mutex_unlock(&gMutex);

    return ApiSuccess;
}


int main() {
    int             rc = A_OK;
    PXL_RETURN_CODE pxlRc = A_OK;
    HANDLE          hCamera = NULL;
    U32             uNumberOfCameras = 0;

    struct timespec startTime;
    struct timespec currentTime;
    F32             runTime;

    int  lastLostFrameCount = gLostFrameCount;
    
    //
    // Step 1
    //      Do some initial setup
    if (pthread_mutex_init(&gMutex, NULL) != 0)
    {
        printf ("Error:  Cannot create mutex\n");
        rc = GENERAL_ERROR;
        goto AllDone;
    }
    fflush(stdin);

    //
    // Step 2
    //      Grab the camera.  For now, grab my specific camera
    //pxlRc = PxLGetNumberCameras (NULL, &uNumberOfCameras);
    //if (!API_SUCCESS(pxlRc) || uNumberOfCameras != 1)
    //{
    //    printf ("Error:  There should be exactly one PixeLINK camera connected. rc:0x%x uNumberOfCameras:%d\n", pxlRc, uNumberOfCameras);
    //    rc = GENERAL_ERROR;
    //    goto AllDone;
    //}
    pxlRc = PxLInitialize (0, &hCamera);
    if (!API_SUCCESS(pxlRc))
    {
        printf ("Error:  Could not initialize the camera.rc:0x%X\n", pxlRc);
        rc = GENERAL_ERROR;
        goto AllDone;
    }

    //
    // Step 3
    //      Set the camera up to use callbacks and start the stream
    rc = PxLSetCallback(hCamera, CALLBACK_FRAME, NULL, FrameCallbackFunction);
    if (!API_SUCCESS(rc)) {
        printf("Error: Could not set the frame callback\n");
        rc = GENERAL_ERROR;
        goto AllDone;
    }
    pxlRc = PxLSetStreamState (hCamera, START_STREAM);
    if (!API_SUCCESS(rc)) {
        printf("Error: Could not start the stream\n");
        rc = GENERAL_ERROR;
        goto AllDone;
    }

    //
    // Step 4
    //      Report on frameCount / frameLoss until the user wants to quit
    printf ("   Looking for lost frames.  Press any key to exit\n");
    clock_gettime(CLOCK_REALTIME, &startTime);
    while (true)
    {
        if (kbhit()) break;
        clock_gettime(CLOCK_REALTIME, &currentTime);
        runTime = (currentTime.tv_sec - startTime.tv_sec) +
                  (currentTime.tv_nsec - startTime.tv_nsec) / BILLION;
        if (lastLostFrameCount != gLostFrameCount) printf ("\n");
        printf ("      %8.2f RxFrames: %d LostFrames: %d\r", runTime, gFrameCount, gLostFrameCount);
        lastLostFrameCount = gLostFrameCount;
        usleep (3 * 1000); // Delay 10 ms, just to be kind to the CPU
    }

    // 
    // Step 5
    //      Stop the camera stream and do some cleanup
AllDone:
    if (hCamera)
    {
        PxLSetStreamState(hCamera, STOP_STREAM);
        PxLSetCallback(hCamera, CALLBACK_FRAME, NULL, NULL);
        PxLUninitialize (hCamera);
    }
    pthread_mutex_destroy(&gMutex);

}

