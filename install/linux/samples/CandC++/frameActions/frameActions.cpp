/*
 * frameActions.cpp
 *
 * Demonstration application showing how to use Frame Action type Triggering
 *
 * Frame action trigging is particularly useful when you need to grab images
 * from multiple cameras simultanesouly -- either at a time as directed by
 * a host appliction (kind of like a multi-camera softwre trigger), or at
 * a user specified time.
 *
 *
 * This particular example requires there be 2 more more cameras connected, and
 * that these cameras support PTP (Percersion Time Protocol V2, AKA IEEE1588), and
 * that the time is synchronized between the cameras (all cameras are eitehr a PTP 
 * Master or Slave).
 */
#include "PixeLINKApi.h"

#include <unistd.h>
#include <stdio.h>
#include <map>

#define EXIT_SUCCESS    0
#define EXIT_FAILURE    1

#define ACTION_DELAY (5.0f) // 5 seconds

using namespace std;

// Use a global mapping to keep track of camera serail numbers (by camera handle)
static	map<HANDLE,U32>gSerials;

static U32
FrameCallbackFunc(
    HANDLE            hCamera,
    void*             pFrameData,
    U32               dataFormat,
    const FRAME_DESC* pFrameDesc,
    void*             context)
{
    // We will print out the frame time.  If all of the cameras are identically configured, then the
    // frame time of each of the images should be identical.  However, if the cameras are of different
    // models, or are configured differently, then the frame time will vary.  The frame time is 
    // 'latched' as the first pixel of the image is received from the imaging sensor, which 
    // would be after the camera's exposure (AKA shutter) time.
    //
    // For example, say we had 2 identical models of camera, each configured identically except for
    // exposure; Camera A is using an exposure of 20 ms, while camera B is using an exposure of 120 ms.
    // In this example, you can expect the timestamps of the images returned from camera A and camera B
    // to differ by 100 ms.
    printf("       FrameCallbackFunction: camera=%09d, exposure=%6.2f (ms) frameTime=%6.3f\n", 
           gSerials[hCamera], pFrameDesc->Shutter.fValue * 1000.0f, pFrameDesc->dFrameTime);
	return ApiSuccess;
}

static bool ptpSynchronized (HANDLE hCamera)
{
    U32 flags = 0;
    U32 numParams = FEATURE_PTP_NUM_PARAMS;
    float ptpParams[FEATURE_PTP_NUM_PARAMS];

    PXL_RETURN_CODE rc = PxLGetFeature (hCamera, FEATURE_PTP, &flags, &numParams, ptpParams);
    if (API_SUCCESS(rc))
    {
        if (ptpParams[FEATURE_PTP_PARAM_STATUS] == FEATURE_PTP_STATUS_MASTER ||
            ptpParams[FEATURE_PTP_PARAM_STATUS] == FEATURE_PTP_STATUS_SLAVE)
        {
            return true;
        }
    }

    return false;
}

int main(const int argc, char const * const argv[])
{
    //
    // Step 1
    //      Ensure there at least 2 cameras connected
    U32 numCameras = 0;
    U32 numPtpSynchedCameras = 0;
	PXL_RETURN_CODE rc = PxLGetNumberCamerasEx (NULL, &numCameras);
	if (!API_SUCCESS(rc) || numCameras < 2) {
		printf("ERROR: There must be at least 2 cameras connected (0x%8.8X)\n", rc);
		return EXIT_FAILURE;
	}
        CAMERA_ID_INFO* cameraIds = new CAMERA_ID_INFO[numCameras]; 
	rc = PxLGetNumberCamerasEx (cameraIds, &numCameras);
	if (!API_SUCCESS(rc) || numCameras < 2) {
		printf("ERROR: Could not find any cameras (0x%8.8X)\n", rc);
		return EXIT_FAILURE;
	}

    U32 numParams = FEATURE_TRIGGER_NUM_PARAMS;
    float triggerParams[FEATURE_TRIGGER_NUM_PARAMS] = {};

    // For each of the ptp synchronized cameras...
    HANDLE hCamera = NULL;
    HANDLE* myCameras = new HANDLE [numCameras];
    for (U32 i=0; i<numCameras; i++)
    {
        myCameras[i] = NULL;
        //
        // Step 2
        //      Initialize the camera, and check to see if it is PTP synchronized
        rc = PxLInitialize (cameraIds[i].CameraSerialNum, &hCamera);
        if (API_SUCCESS(rc))
        {
            if (ptpSynchronized (hCamera))
            {
                numPtpSynchedCameras++;
                gSerials[hCamera] = cameraIds[i].CameraSerialNum;
                //
                // Step 3
                //      Register a frame callback for this camera
                rc = PxLSetCallback(hCamera, CALLBACK_FRAME, NULL, FrameCallbackFunc);
                if (!API_SUCCESS(rc)) printf ("PxLSetCallback rc:0x%08X\n", rc); // Error handling

                // 
                // Step 4
                //      Set up a Frame Action Trigger
                triggerParams[FEATURE_TRIGGER_PARAM_TYPE] = TRIGGER_TYPE_ACTION;
                rc = PxLSetFeature (hCamera, FEATURE_TRIGGER, FEATURE_FLAG_MANUAL, numParams, triggerParams);
                if (!API_SUCCESS(rc))printf ("PxLSetFeature Trigger rc:0x%08X\n", rc); // Error handling

                // 
                // Step 5
                //      Start the stream
                rc = PxLSetStreamState (hCamera, START_STREAM);
                if (!API_SUCCESS(rc))printf ("PxLSetStreamState rc:0x%08X\n", rc); // Error handling
            }
            myCameras[i] = hCamera;  // 'remember' this camera
        }
    }

    printf ("Found %d PTP synched cameras\n", numPtpSynchedCameras);
    
    //
    // Step 5
    //      All ptp synchronized cameras are waiting for a frame event -- send one now.
    printf ("   You should see one image from each of %d cameras, immediatly...\n", numPtpSynchedCameras);
    rc = PxLSetActions (ACTION_FRAME_TRIGGER, 0);

    // 
    // Step 6
    //      All ptp synchronized cameras are waiting for a frame event -- send one that matures in ACTION_DELAY seconds.
    printf ("   You should see one image from each of %d cameras, after %3.1f seconds...\n", numPtpSynchedCameras, ACTION_DELAY);
    double currentTime;
    rc = PxLGetCurrentTimestamp (hCamera, &currentTime);
    if (API_SUCCESS(rc))
    {
        rc = PxLSetActions (ACTION_FRAME_TRIGGER, currentTime+ACTION_DELAY);
    }
    // Delay waiting for the action
    sleep ((int)ACTION_DELAY * 2);

    //
    // Step 7
    //      Cleanup
    for (U32 i=0; i<numCameras; i++)
    {
        PxLSetStreamState (myCameras[i], STOP_STREAM);
        PxLSetFeature (myCameras[i], FEATURE_TRIGGER, FEATURE_FLAG_OFF, numParams, triggerParams);
        PxLUninitialize (myCameras[i]);
    }
    
    return EXIT_SUCCESS;
}

