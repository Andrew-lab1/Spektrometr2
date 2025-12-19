//
// This demonstrates how to control a camera's general purpose input (gpi), and general purpose output (gpo).
//
// This program will setup the first GPIO as a 'input', and the second GPO as an 'output'.  It will then
// poll the GPI value, reporting on the value on the console, and on the GPO.
//

#include "PixeLINKApi.h"
#include "LinuxUtil.h"

#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

//
// A few useful defines and enums.
//
#define GpioState bool
#define GPIO_ON  true
#define GPIO_OFF false

// The poll period of the GPI, in microseconds
#define POLL_PERIOD_US (500 * 1000)

// returns true if the camera has a GPI and a GPO.
static bool supportsGpio (HANDLE hCamera) {
    U32  bufferSize = 0;
    U32  rc;
    PCAMERA_FEATURES pGpioFeatureInfo;

    //  Step 1.
    //       Get info on the GPIO for this camera
    rc = PxLGetCameraFeatures (hCamera, FEATURE_GPIO, NULL, &bufferSize);
    if (!API_SUCCESS(rc) || bufferSize == 0) return false;
    pGpioFeatureInfo = (PCAMERA_FEATURES) malloc (bufferSize);
    rc = PxLGetCameraFeatures (hCamera, FEATURE_GPIO, pGpioFeatureInfo, &bufferSize);
    if (!API_SUCCESS(rc) || bufferSize == 0) return false;

    // Step 2.
    //      Look at the feature info, and ensure the camera supports a GPI and a GPO.
    if (! (pGpioFeatureInfo->pFeatures->uFlags & FEATURE_FLAG_PRESENCE)) return false;  // No GPIOs at all !!
    if (pGpioFeatureInfo->pFeatures->pParams[0].fMaxValue < 2.0f) return false;         // We need at least 2 GPIO !!
    if (pGpioFeatureInfo->pFeatures->pParams[1].fMaxValue < GPIO_MODE_INPUT) return false; // Does not support GPI !!

    return true;
}

static U32 setupGpios (HANDLE hCamera) {
    U32       rc;
    float     gpioParams[6];

    // Step 1
    //      Setup GPIO #1 for input
    gpioParams[FEATURE_GPIO_PARAM_GPIO_INDEX] = 1.0; // The first GPIO
    gpioParams[FEATURE_GPIO_PARAM_MODE]       = (float)GPIO_MODE_INPUT; 
    gpioParams[FEATURE_GPIO_PARAM_POLARITY]   = (float)0;               
    // Don't care about the other parameters
    rc = PxLSetFeature (hCamera, FEATURE_GPIO, FEATURE_FLAG_MANUAL, 6, gpioParams);
    if (!API_SUCCESS(rc)) return rc;

    // Step 1
    //      Setup GPIO #2 for manual output
    gpioParams[FEATURE_GPIO_PARAM_GPIO_INDEX] = 2.0; // The second GPIO
    gpioParams[FEATURE_GPIO_PARAM_MODE]       = (float)GPIO_MODE_NORMAL; 
    gpioParams[FEATURE_GPIO_PARAM_POLARITY]   = (float)0;     
    // Don't care about the other parameters
    rc = PxLSetFeature (hCamera, FEATURE_GPIO, FEATURE_FLAG_MANUAL, 6, gpioParams);

    return rc;
}


int main() {
    HANDLE myCamera;

    DontWaitForEnter unbufferedKeyboard;  // Declare this for our getchar
    GpioState currentGpi;
    GpioState lastGpi;
    U32       flags = 0;
    U32       numParams = 6;
    float     gpioParams[6];

    ULONG rc;

    // Step 1.
    //      Find and initialize a camera.
    rc = PxLInitializeEx(0,&myCamera,0);
    if (!API_SUCCESS(rc))
    {
        printf ("Could not Initialize the camera!  Rc = 0x%X\n", rc);
        return 1;
    }

    //
    // Step 2.
    //      Ensure the camera has at least 2 gpos, and supports a GPI (which is always the first GPIO).
    if (!supportsGpio(myCamera))
    {
        printf ("Camera does not support GPIO!\n");
        PxLUninitialize(myCamera);
        return 1;
    }

    // Step 3.
    //      Set the camera up for one GPI and one GPO
    rc = setupGpios(myCamera);
    if (!API_SUCCESS(rc))
    {
        printf ("Could not setup the GPIOs!  Rc = 0x%X\n", rc);
        PxLUninitialize(myCamera);
        return 1;
    }
    
    // give the last GPI a value that will ensure our loop will print/assert the GPO on its first time through
    lastGpi = GPIO_ON;

    // Step 4.
    //      Poll the GPI (the first GPIO), reporting it's value on the console, and on the GPO (the second GPIO)
    
    printf ("Polling the GPI every %d ms, press any key to exit\n", POLL_PERIOD_US/1000);
    while (!kbhit())
    {
        // Read the GPI
        gpioParams[FEATURE_GPIO_PARAM_GPIO_INDEX] = 1.0;
        rc = PxLGetFeature (myCamera, FEATURE_GPIO, &flags, &numParams, gpioParams);
	if (!API_SUCCESS(rc))
        {
           printf ("\nCould not read the GPI!  Rc = 0x%X\n", rc);
           PxLUninitialize(myCamera);
           return 1;
        }
        currentGpi = gpioParams[FEATURE_GPIO_MODE_INPUT_PARAM_STATUS] == 0.0 ? GPIO_OFF : GPIO_ON;

        // If the GPI changed, then set the GPO
        
        if (currentGpi != lastGpi)
        {
           printf ("\rGPI is %s   ", currentGpi == GPIO_ON ? "On" : "Off");
           fflush(stdout);

           gpioParams[FEATURE_GPIO_PARAM_GPIO_INDEX] = 2.0;
           gpioParams[FEATURE_GPIO_PARAM_MODE]       = (float)GPIO_MODE_NORMAL; 
           gpioParams[FEATURE_GPIO_PARAM_POLARITY]   = (float)currentGpi;     
           // Don't care about the other parameters
           rc = PxLSetFeature (myCamera, FEATURE_GPIO, FEATURE_FLAG_MANUAL, 6, gpioParams);
           if (!API_SUCCESS(rc))
           {
              printf ("\nCould not write the GPO!  Rc = 0x%X\n", rc);
              PxLUninitialize(myCamera);
              return 1;
           }
           
           lastGpi = currentGpi;
        }

        usleep (POLL_PERIOD_US); 

    }
    printf ("\n");

    PxLUninitialize (myCamera);
}

