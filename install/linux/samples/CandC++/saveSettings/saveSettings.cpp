//
// This demonstrates how save camera settings in non-volatile store, as well as restoring
// a camera back to using its factory default settings.
//
// More specifically, is howcases how to use:
//    - PxLSaveSettings
//    - PxLLoadSettings
//
// NOTE: This application assumes there is at most, one PixeLINK camera connected to the system

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
#define WAIT_TIME     30000 // total wait time for a camera to finish a power cycle (in milliseconds)
#define POLL_INTERVAL 1000   // Time betwen polls to see if a camera has finished a power cycle (in milliseconds)

// block until the user presses a key, and then return the key pressed
int getAKeystroke() {

    DontWaitForEnter unbufferedKeyboard;  // Declare this for our getchar
    int   keyPressed;
    bool  done = false;

    // I prefer unbuffered keyboard input, where the user is not required to press enter
    while (!done)
    {
        fflush(stdin);
        if (kbhit())
        {
            done = true;
            keyPressed = getchar();
        }
        usleep (100*1000);  // 100 ms - don't hog all of the CPU
    }

    return keyPressed;
}

int main() {

    HANDLE myCamera;

    ULONG rc;

    float currentExposure;
    float factoryDefaultExposure;
    ULONG numParams = 1;
    ULONG flags;

    int waitTime;
    int keyPressed;

    //
    // Step 1
    //      Find, and initialize, a camera
    rc = PxLInitializeEx (0,&myCamera,0);
    if (!API_SUCCESS(rc))
    {
        printf ("Could not Initialize the camera!  Rc = 0x%X\n", rc);
        return 1;
    }

    printf ("\nWARNING: This application will restore the connected camera back to it's factory default settings\n");
    printf ("   Ok to proceed (y/n)? ");
    fflush(stdout);

    keyPressed = getAKeystroke ();
    printf("\n");
    if (keyPressed != 'y' && keyPressed != 'Y')
    {
        // User aborted.
        PxLUninitialize (myCamera);
        return 1;
    }

    //
    // Step 2
    //      Restore the camera back to it's factory default settings, and report the expsoure
    rc = PxLLoadSettings (myCamera, PXL_SETTINGS_FACTORY);
    ASSERT (API_SUCCESS(rc));
    rc = PxLGetFeature (myCamera, FEATURE_EXPOSURE, &flags, &numParams, &factoryDefaultExposure);
    ASSERT (API_SUCCESS(rc));
    printf (" Exposure: %7.2f ms <-- factory default value.\n", factoryDefaultExposure*1000.0);

    // Step 3.
    //      Change the exposure.  We'll choose a value that is 10% longer, and save the camera settings
    //      to non-volatile store.
    currentExposure = factoryDefaultExposure * 1.1;
    rc = PxLSetFeature (myCamera, FEATURE_EXPOSURE, FEATURE_FLAG_MANUAL, numParams, &currentExposure);
    ASSERT (API_SUCCESS(rc));
    rc = PxLSaveSettings (myCamera, 1); // Channel 0 is reserved for factory defaults
    printf (" PxLSaveSettings returned 0x%x\n", rc);

    //
    // Step 4.
    //      Have the user power cycle the camera.

    // Do an uninitialize of the camera first, so some cleanup can be done.
    PxLUninitialize (myCamera);
    printf (" Exposure: %7.2f ms <-- user set value.  Please power cycle the camera.  Press a key when you are done. ",
              currentExposure*1000.0);
    fflush(stdout);
    getAKeystroke ();
    printf("\nWaiting for the camera to finish initializing");
    fflush(stdout);

    //
    // Step 5.
    //      Wait for the camera to reappear, but don't wait forever
    waitTime = 0;
    while (waitTime < WAIT_TIME)
    {
        rc = PxLInitializeEx (0,&myCamera,0);
        if (API_SUCCESS(rc)) break;
        printf(".");
        fflush(stdout);
        // Recheck for the camera from time to time
        waitTime += POLL_INTERVAL;
        usleep (POLL_INTERVAL*1000);  // don't hog all of the CPU
    }
    printf("done\n");
    ASSERT (waitTime < WAIT_TIME);

    //
    // Step 6.
    //      Report the camera's exposure.  It should still be the user set value
    rc = PxLGetFeature (myCamera, FEATURE_EXPOSURE, &flags, &numParams, &currentExposure);
    ASSERT (API_SUCCESS(rc));
    printf (" Exposure: %7.2f ms <-- non-volatile user set value.\n", currentExposure*1000.0);
    ASSERT (currentExposure != factoryDefaultExposure);

    //
    // Step 7.
    //      Restore the camera back to factory defaults, and then save these defaults to
    //      non-volatile store.
    rc = PxLLoadSettings (myCamera, PXL_SETTINGS_FACTORY);
    printf (" PxLLoadSettings returned 0x%x\n", rc);
    rc = PxLGetFeature (myCamera, FEATURE_EXPOSURE, &flags, &numParams, &factoryDefaultExposure);
    ASSERT (API_SUCCESS(rc));
    rc = PxLSaveSettings (myCamera, 1); // Channel 0 is reserved for factory defaults
    ASSERT (API_SUCCESS(rc));

    //
    // Step 8.
    //      Have the user power cycle the camera.
    // Do an uninitialize of the camera first, so some cleanup can be done.
    PxLUninitialize (myCamera);
    printf (" Exposure: %7.2f ms <-- factory default value. Please power cycle the camera and press a key when you are done. ",
            factoryDefaultExposure*1000.0);
    fflush(stdout);
    getAKeystroke ();
    printf("\nWaiting for the camera to finish initializing");
    fflush(stdout);

    //
    // Step 9.
    //      Wait for the camera to reappear, but don't wait forever
    waitTime = 0;
    while (waitTime < WAIT_TIME)
    {
        rc = PxLInitializeEx (0,&myCamera,0);
        if (API_SUCCESS(rc)) break;
        printf(".");
        fflush(stdout);
        // Recheck for the camera from time to time
        waitTime += POLL_INTERVAL;
        usleep (POLL_INTERVAL*1000);  // don't hog all of the CPU
    }
    printf("done\n");
    ASSERT (waitTime < WAIT_TIME);

    //
    // Step 10.
    //      Report the camera's exposure.  It should still be the factory default value again
    rc = PxLGetFeature (myCamera, FEATURE_EXPOSURE, &flags, &numParams, &currentExposure);
    ASSERT (API_SUCCESS(rc));
    printf (" Exposure: %7.2f ms <-- non-volatile factory default value.\n", currentExposure*1000.0);
    ASSERT (currentExposure == factoryDefaultExposure);

    //
    // Step 11.
    //      Done.
    PxLUninitialize (myCamera);
    return 0;
}

