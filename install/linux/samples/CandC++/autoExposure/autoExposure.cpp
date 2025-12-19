//
// This demonstrates how to control a camera's autoexposure features.
//
// Option 1) Enable autoexposure (FEATURE_FLAG_AUTO)
// When autoexposure is enabled, the camera controls the exposure based on its own internal algorithm.
// The exposure will be continually adjusted over time by the camera until autoexposure is disabled.
//
// Option 2) Autoexpose Once (FEATURE_FLAG_ONEPUSH)
// When initiated, the camera will adjust the exposure based on its own internal algorithm. Once
// a satisfactory exposure has been determined, the camera will release control of the exposure whereupon
// the exposure is now again settable via the SDK.
//
// Option 3) User/applicaton control (FEATURE_FLAG_MANUAL)
// The camera will not make any adjustments to the exposure 'automatically'l it will only set
// the exposure to a specified value, when told to do so by the application.
//
// With autoexpose once, this application demonstrates how you can either:
// A) Initiate autoexpose once, then start polling the camera, looping until the operation is complete.
// B) Initiate autoexpose once, and then poll the camera at a regular interval with a timer.
//
// Note that this sample applicationb uses the pthread library to spawn off a thread to perform a one time
// auto exposure.
//

#include "PixeLINKApi.h"
#include "LinuxUtil.h"

#include <iostream>
#include <stdio.h>
#include <stdexcept>
#include <unistd.h>
#include <cassert>
#include <vector>
#include <pthread.h>

using namespace std;

//
// A few useful defines and enums.
//
#define ASSERT(x)	do { assert((x)); } while(0)
#define API_RANGE_ERROR(rc) (((PXL_RETURN_CODE)rc == ApiInvalidParameterError) || ((PXL_RETURN_CODE)rc == ApiOutOfRangeError))

typedef enum _OneTimeState
{
    Inactive,    // A one time operation is NOT in progress
    InProgress,  // A one time operation IS in progress
    Stopping     // A one time operation IS in progress, but an abort request has been made.
} OneTimeState;

// Global to remember the expsoure limits.  Set with a succesful call to cameraSupportsAutoFeature
// Note that this application doesn't actually use these -- but your application might find this
// this information useful.
static FEATURE_PARAM exposureLimits;

static OneTimeState oneTimeAutoExpose = Inactive;
static pthread_t    oneTimeThread;

// Returns true if the camera supports one-time auto adjustment and continual adjustment of the specified feature,
// false otherwise
static bool cameraSupportsAutoFeature (HANDLE camera, U32 featureId)
{
    // How big a buffer will we need to hold the information about the feature?
    U32 bufferSize = -1;
    PXL_RETURN_CODE rc = PxLGetCameraFeatures(camera, featureId, NULL, &bufferSize);
    ASSERT(API_SUCCESS(rc));
    ASSERT(bufferSize > 0);

    // Declare a buffer and read the feature information
    vector<U8> buffer(bufferSize, 0);  // zero-initialized buffer
    CAMERA_FEATURES* pCameraFeatures = (CAMERA_FEATURES*)&buffer[0];
    rc = PxLGetCameraFeatures(camera, featureId, pCameraFeatures, &bufferSize);
    ASSERT(API_SUCCESS(rc));

    // Check the sanity of the return information
    ASSERT(1 == pCameraFeatures->uNumberOfFeatures);			// We only asked about one feature...
    ASSERT(featureId == pCameraFeatures->pFeatures[0].uFeatureId);	// ... and that feature is the one requested
    bool isSupported = ((pCameraFeatures[0].pFeatures->uFlags & FEATURE_FLAG_PRESENCE) != 0);
    bool supportsOneTimeAuto = ((pCameraFeatures[0].pFeatures->uFlags & FEATURE_FLAG_ONEPUSH) != 0);
    bool supportsContinualAuto = ((pCameraFeatures[0].pFeatures->uFlags & FEATURE_FLAG_AUTO) != 0);

    if (isSupported && supportsOneTimeAuto && supportsContinualAuto)
    {
        // This app does not need/use these -- but yours might....
        exposureLimits   = pCameraFeatures[0].pFeatures->pParams[0];
    }

    return isSupported && supportsOneTimeAuto && supportsContinualAuto;
}

// thread to perform a one-time auto expose operation, ending when the operation is complete, or
// until told to abort.
static void *performOneTimeAutoExposure (HANDLE myCamera)
{
    ULONG rc;
    U32   flags;
    U32   numParams = 1;
    float exposure = 0.0; // Intialize to 0, but this value is ignored when initating auto adjustment.

    oneTimeAutoExpose = InProgress;
    printf ("Starting one time auto exposure adjustment.\n");

    rc = PxLSetFeature (myCamera, FEATURE_EXPOSURE, FEATURE_FLAG_ONEPUSH, numParams, &exposure);
    if (!API_SUCCESS(rc))
    {
       printf (" !! Attempt to set auto exposure returned 0x%X!\n", rc);
       oneTimeAutoExpose = Inactive;
       return NULL;
    }

    // Now that we have initiated a one time operation, loop until it is done (or told to abort).
    while (oneTimeAutoExpose == InProgress)
    {
        rc = PxLGetFeature (myCamera, FEATURE_EXPOSURE, &flags, &numParams, &exposure);
        if (API_SUCCESS(rc))
        {
            if (!(flags & FEATURE_FLAG_ONEPUSH)) break;  //Whoo-hoo -- the operation completed.
        }
        usleep (250*1000);  // 250 ms - Give some time for the one time operation to complete
    }

    printf ("Finished one time auto exposure adjustment. %s\n",
            oneTimeAutoExpose == Stopping ? "Operation aborted.                " :
                                            "Operation completed successfully. ");
    oneTimeAutoExpose = Inactive;

    return NULL;
}

// If a one-time auto expose is not already in progress, starts a thread that will perfrom the
// one time auto expsoure, exiting when it has completed (or is aborted).
static void initiateOneTimeAutoExposure (HANDLE myCamera)
{
    if (oneTimeAutoExpose == Inactive)
    {
       if (pthread_create (&oneTimeThread, NULL, performOneTimeAutoExposure, myCamera))
       {
           printf ("  !! Could not create a one time auto expsore thread!\n");
       }
    }
}

// If a one-time auto expose is in progress, stops the thread that is perfroming the
// one time auto expsoure, waiting for the thread to exit.
static void abortOneTimeAutoExposure (HANDLE myCamera)
{
    if (oneTimeAutoExpose == InProgress)
    {
        oneTimeAutoExpose = Stopping;
        pthread_join (oneTimeThread, NULL);
    }
}

// Manually adjusts the expsoure, increasing or decreaing it by 10%
static void bumpExposure (HANDLE camera, bool up)
{
    ULONG rc;
    U32   flags;
    U32   numParams = 1;
    float exposure;

    rc = PxLGetFeature (camera, FEATURE_EXPOSURE, &flags, &numParams, &exposure);
    if (!API_SUCCESS(rc))
    {
        printf (" !! Attempt to get expsoure returned 0x%X!\n", rc);
        return;
     }

    if (up) exposure *= 1.1;
    else    exposure /= 1.1;

    rc = PxLSetFeature (camera, FEATURE_EXPOSURE, FEATURE_FLAG_MANUAL, numParams, &exposure);
    if (!API_SUCCESS(rc) && !API_RANGE_ERROR(rc))
    {
       printf (" !! Attempt to set auto exposure returned 0x%X!\n", rc);
    }
}

// Initiates a one-time auto adjustment of the white balance.  Note that this routine does not wait for the operation to complete.
static void setContinualAutoExposure (HANDLE camera, bool on)
{
    ULONG rc;
    U32   flags;
    U32   numParams = 1;
    float exposure = 0.0; // Intialize to 0, but this value is ignored when initating auto adjustment.

    if (!on)
    {
        // We are looking to turn off the continual auto exposure, which means we will be manually
        // adjusting it from now on -- including with the below PxLSetFeature.  So given that we have to
        // set it to someting, read the current value (as set by the camera), and use that value.
        rc = PxLGetFeature (camera, FEATURE_EXPOSURE, &flags, &numParams, &exposure);
        if (!API_SUCCESS(rc))
        {
           printf (" !! Attempt to get expsoure returned 0x%X!\n", rc);
           return;
        }
        flags = FEATURE_FLAG_MANUAL;
    } else {
        flags = FEATURE_FLAG_AUTO;
    }

    rc = PxLSetFeature (camera, FEATURE_EXPOSURE, flags, numParams, &exposure);
    if (!API_SUCCESS(rc))
    {
	   printf (" !! Attempt to set auto exposure returned 0x%X!\n", rc);
    }
}

// Prints out the current exppsoure value, and a message if the camera is currently adjusting them.
static void printExposure (HANDLE camera)
{
    ULONG rc;
    U32   flags;
    U32   numParams = 1;
    float exposure;

    rc = PxLGetFeature (camera, FEATURE_EXPOSURE, &flags, &numParams, &exposure);
    ASSERT(API_SUCCESS(rc));

    printf ("\rExposure:%6.1f millseconds, Adjustment type:%s",
            exposure*1000.0,
            flags & FEATURE_FLAG_AUTO ?    "Continuous " :
           (flags & FEATURE_FLAG_ONEPUSH ? "One time   " :
                                           "Manual     "));
}

int main() {
    HWND  previewWin;
    HANDLE myCamera;

    DontWaitForEnter unbufferedKeyboard;  // Declare this for our getchar
    int   keyPressed;
    bool  done = false;

    PXL_RETURN_CODE rc;

    rc = PxLInitializeEx(0,&myCamera,0);
    if (!API_SUCCESS(rc))
    {
        printf ("Could not Initialize the camera!  Rc = 0x%X\n", rc);
        return 1;
    }
    if (ApiSuccessApiLiteWarning == rc)
    {
        printf ("This system is currently using the Lite variant of the Pixelink API,\n"
                "but this application requires the standard variant\n");
        PxLUninitialize(myCamera);
        return 1;
    }

    if (! cameraSupportsAutoFeature(myCamera, FEATURE_EXPOSURE))
    {
        printf ("Camera does not support Auto Expsoure\n");
        PxLUninitialize(myCamera);
        return 1;
    }

    printf ("Starting the stream for camera with handle:%p\n", myCamera);
    printf ("    q   : to quit\n");
    printf ("    +   : to increase exposure by 10%%\n");
    printf ("    -   : to decrease expsoiure by 10%%\n");
    printf ("    c   : to turn ON continual auto exposure\n");
    printf ("    C   : to turn OFF continual auto exposure (so will +, -, or o)\n");
    printf ("    o   : to perform a one time auto expsoure operation\n\n");
    printf ("NOTE: pressing any key will abort any one time auto exposure in progress\n\n");

    // Starting the stream will instantiate the the BufferControl -- so do it before I start the preview
    rc = PxLSetStreamState (myCamera, START_STREAM);
    if (!API_SUCCESS(rc))
    {
        printf ("Could not start the stream! Rc = 0x%X\n", rc);
        PxLUninitialize(myCamera);
        return 1;
    }

    rc = PxLSetPreviewState (myCamera, START_PREVIEW, &previewWin);
    if (!API_SUCCESS(rc))
    {
        printf ("Could not start the preview! Rc = 0x%X\n", rc);
        PxLSetStreamState (myCamera, STOP_STREAM);
        PxLUninitialize(myCamera);
        return 1;
    }

    while (!done)
    {
        fflush(stdin);
        if (kbhit())
        {
            keyPressed = getchar();
            abortOneTimeAutoExposure (myCamera);
            switch (keyPressed)
            {
            case 'q':
            case 'Q':
                printf ("\n\n");
                done = true;
                break;
            case '+':
            case '-':
                bumpExposure (myCamera, keyPressed == '+');
                break;
            case 'c':
                setContinualAutoExposure (myCamera, true);
                break;
            case 'C':
                setContinualAutoExposure (myCamera, false);
                break;
            case 'o':
            case 'O':
		        initiateOneTimeAutoExposure (myCamera);
		        break;
            }
            fflush(stdout);
        }
        if (!done)
        {
            printExposure (myCamera);
            fflush(stdout);
            usleep (100*1000);  // 100 ms - don't hog all of the CPU
        }
    }

    PxLSetPreviewState(myCamera, STOP_PREVIEW, &previewWin);

    PxLSetStreamState (myCamera, STOP_STREAM);  

    PxLUninitialize (myCamera);
}

