//
// whiteBalance.cpp
//
// Demonstrates how you can control the white balance on a (color)
// camera.  Furthermore, it also can instruct the camera to perform
// automatic white balance.
//
// Note 1: The sample application 'autoExppsure' also shows how to control
// camera features that support continual and one-time auto adjustments
// (such as white balance).  However, that particular sample will also
// show how to cancel these auto adjustments.
// 
#include "PixeLINKApi.h"
#include "LinuxUtil.h"

#include <iostream>
#include <stdio.h>
#include <stdexcept>
#include <unistd.h>
#include <vector>
#include <cassert>

using namespace std;

//
// A few useful defines and enums.
//
#define ASSERT(x)	do { assert((x)); } while(0)

typedef enum _ColorChannels
{
    RedChannel,
    GreenChannel,
    BlueChannel,
    NumColorChannels = BlueChannel
} ColorChannels;

// Globals to remember the color gain limits.  Set with a succesful call to cameraSupportsFeature
static FEATURE_PARAM colorLimits[NumColorChannels];

// Returns true if the camera supports one-time auto adjustment of the specified feature, false otherwise
static bool cameraSupportsFeature (HANDLE camera, U32 featureId)
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

    if (isSupported && supportsOneTimeAuto)
    {
        colorLimits[RedChannel]   = pCameraFeatures[0].pFeatures->pParams[RedChannel];
        colorLimits[GreenChannel] = pCameraFeatures[0].pFeatures->pParams[GreenChannel];
        colorLimits[BlueChannel]  = pCameraFeatures[0].pFeatures->pParams[BlueChannel];
    }
    return isSupported && supportsOneTimeAuto;
}

// Changes the specified color channel - increasing or decreasing it's value by a fixed amount.
static void changeChannel (HANDLE camera, ColorChannels color, bool increase)
{
    ULONG rc;
    U32   flags;
    U32   numParams = 3;
    vector<float> cameraColors(numParams);

    // Get the current color settings
    rc = PxLGetFeature (camera, FEATURE_WHITE_SHADING, &flags, &numParams, &cameraColors[0]);
    ASSERT(API_SUCCESS(rc));

    // adjust the specified color
    if (increase)
       cameraColors[color] += cameraColors[color]*0.1;
    else
       cameraColors[color] -= cameraColors[color]*0.1;

    // set the new color value, if it's in range
    if (cameraColors[color] > colorLimits[color].fMinValue &&
        cameraColors[color] < colorLimits[color].fMaxValue)
    {
        rc = PxLSetFeature (camera, FEATURE_WHITE_SHADING, flags, numParams, &cameraColors[0]);
        if (!API_SUCCESS(rc))
        {
	    printf (" !! Attempt to set White Balance to R:%f, G:%f, B:%f returned 0x%X!\n", 
                    cameraColors[0], cameraColors[1], cameraColors[2], rc);
        }
    }
}

// Initiates a one-time auto adjustment of the white balance.  Note that this routine does not wait for the operation to complete.
static void autoWhiteBalance (HANDLE camera)
{
    ULONG rc;
    U32   numParams = 3;
    vector<float> cameraColors(numParams, 0.0); // Intialize to 0 (no color), but these values are ignored when initating auto adjustment.

    rc = PxLSetFeature (camera, FEATURE_WHITE_SHADING, FEATURE_FLAG_ONEPUSH, numParams, &cameraColors[0]);
    if (!API_SUCCESS(rc))
    {
	printf (" !! Attempt to set Auto White Balance returned 0x%X!\n", rc);
    }
}

// Prints out the current color channel values, or a message if the camera is currently adjusting them.
static void printColorChannels (HANDLE camera)
{
    ULONG rc;
    U32   flags;
    U32   numParams = 3;
    vector<float> cameraColors(numParams);

    rc = PxLGetFeature (camera, FEATURE_WHITE_SHADING, &flags, &numParams, &cameraColors[0]);
    ASSERT(API_SUCCESS(rc));

    // Is an auto adjustment still in progress?
    if (flags & FEATURE_FLAG_ONEPUSH)
    {
        printf ("-- Camera is auto adjusting --         \n");
        return;
    }

    printf ("\rGains --> red:%4.2f green:%4.2f blue:%4.2f", 
            cameraColors[0], cameraColors[1], cameraColors[2]);
}

int main() {
    HWND  previewWin;
    HANDLE myCamera;

    DontWaitForEnter unbufferedKeyboard;  // Declare this for our getchar
    int   keyPressed;
    bool  done = false;

    ULONG rc;

    rc = PxLInitializeEx(0,&myCamera,0);
    if (!API_SUCCESS(rc))
    {
        printf ("Could not Initialize the camera!  Rc = 0x%X\n", rc);
        return 1;
    }

    if (! cameraSupportsFeature(myCamera, FEATURE_WHITE_SHADING))
    {
        printf ("Camera does not support White Balance\n");
        PxLUninitialize(myCamera);
        return 1;
    }

    printf ("Starting the stream for camera with handle:%p\n", myCamera);
    printf ("    q   : to quit\n");
    printf ("    r/R : to decrease/increase red color channel by 10%%\n");
    printf ("    g/G : to decrease/increase green color channel by 10%%\n");
    printf ("    b/B : to decrease/increase blue color channel by 10%%\n");
    printf ("    a   : Perform a one time auto white balance on the camera\n");

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
            switch (keyPressed)
            {
            case 'q':
            case 'Q':
                printf ("\n\n");
                done = true;
                break;
            case 'r':
		changeChannel (myCamera, RedChannel, false);
		break;
            case 'R':
		changeChannel (myCamera, RedChannel, true);
		break;
            case 'g':
		changeChannel (myCamera, GreenChannel, false);
		break;
            case 'G':
		changeChannel (myCamera, GreenChannel, true);
		break;
            case 'b':
		changeChannel (myCamera, BlueChannel, false);
		break;
            case 'B':
		changeChannel (myCamera, BlueChannel, true);
		break;
            case 'a':
            case 'A':
		autoWhiteBalance (myCamera);
		break;
            }
            fflush(stdout);
        }
        if (!done)
        {
            printColorChannels(myCamera);
            fflush(stdout);
            usleep (100*1000);  // 100 ms - don't hog all of the CPU
        }
    }

    PxLSetPreviewState(myCamera, STOP_PREVIEW, &previewWin);

    PxLSetStreamState (myCamera, STOP_STREAM);  

    PxLUninitialize (myCamera);
}

