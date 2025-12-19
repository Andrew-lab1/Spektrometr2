//
// This demonstrates how to control a camera's general purpose input (gpi)).
//
//

#include "PixeLINKApi.h"
#include "LinuxUtil.h"

#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#define A_OK          0  // non-zero error codes
#define GENERAL_ERROR 1

#define BILLION 1E9

// Prototypes to aloow top-down structure
void usage (char **argv);
int  getParameters (int argc, char* argv[], U32* pollPeriod, bool* invert);

int main(int argc, char* argv[])
{
   U32  uPollPeriod = 50;  // Units are in milliseconds.  Default to poll 20 times/second
	bool bInvert = false;   // Indicates if the GP Input polarity shold be reversed
	
	//
	// Step 1
	//      Validate the user parameters, getting poll period and invert value
    if (A_OK != getParameters(argc, argv, &uPollPeriod, &bInvert))
    {
        usage(argv);
        return GENERAL_ERROR;
    }

	//
	// Step 2
	//		Grab our camera
	HANDLE			hCamera;
	PXL_RETURN_CODE rc = A_OK;
	U32				uNumberOfCameras = 0;

    rc = PxLGetNumberCameras (NULL, &uNumberOfCameras);
	if (!API_SUCCESS(rc) || uNumberOfCameras != 1)
	{
        printf ("Error:  There should be exactly one PixeLINK camera connected.\n");
        return GENERAL_ERROR;
	}
    rc = PxLInitialize (0, &hCamera);
    if (!API_SUCCESS(rc))
    {
        printf ("Error:  Could not initialize the camera.\n");
        return GENERAL_ERROR;
    }

    // 
    // Step 3 (Optional)
    //      If requested, invert the polarity of the input signal
    U32  uFlags;
    F32  fParams[6]; // Make this large enough for trigger ot GPO feature
    U32  uNumParams = 5;  // Trigger uses 5 parameters
    if (bInvert)
    {
        fParams[FEATURE_TRIGGER_PARAM_MODE] = 0.0;
        fParams[FEATURE_TRIGGER_PARAM_TYPE] = (F32)TRIGGER_TYPE_HARDWARE;
        fParams[FEATURE_TRIGGER_PARAM_POLARITY] = 1.0; // <-- setting this to 1 will cause the signal inversion
        fParams[FEATURE_TRIGGER_PARAM_DELAY] = 0.0;
        rc = PxLSetFeature (hCamera, FEATURE_TRIGGER, FEATURE_FLAG_MANUAL, uNumParams, fParams);
        if (!API_SUCCESS(rc)) 
        {
            printf ("Error:  Could not invert the input signal\n");
            goto cleanupAndExit;    
        }     
    }

    //
    // Step 4
    //      Setup the GPIO as an input signal
    uNumParams = 6;  // GPO uses 6 parameters
    fParams[FEATURE_GPIO_PARAM_GPIO_INDEX] = 1.0;  // the first strobe is the one that is tied to the hardware input.
    fParams[FEATURE_GPIO_PARAM_MODE] = (F32)GPIO_MODE_INPUT; // Use input mode
    fParams[FEATURE_GPIO_PARAM_POLARITY] = 0.0; // This must be 0 or 1
    // Be sure to enable the GPO
    rc = PxLSetFeature (hCamera, FEATURE_GPIO, FEATURE_FLAG_MANUAL, uNumParams, fParams);
    if (!API_SUCCESS(rc)) 
    {
        printf ("Error:  Could not enable the General Purpose Input\n");
        goto cleanupAndExit;    
    }

    //
    // Step 5
    //      Continously loop, reporting on the GP Input status
    bool bLastGpiValue;
    bLastGpiValue = false;

    struct timespec startTime;
    struct timespec currentTime;
    F32             runTime;  // Seconds units

    clock_gettime(CLOCK_REALTIME, &startTime);

    printf ("Press any key to exit\n");
    while (true)
    {
        if (kbhit()) break;

        //
        // Step 5.1
        //      Read and report the GP input value
        rc = PxLGetFeature (hCamera, FEATURE_GPIO, &uFlags, &uNumParams, fParams);
        if (!API_SUCCESS(rc))
        {
            printf ("\nError:  Could not read the GPI.\n"); 
            break;
        }
        clock_gettime(CLOCK_REALTIME, &currentTime);
        runTime = (currentTime.tv_sec - startTime.tv_sec) +
                  (currentTime.tv_nsec - startTime.tv_nsec) / BILLION;
        // It is the FEATURE_GPIO_MODE_INPUT_PARAM_STATUS parameter that tells us the input signal value.
        printf ("  %8.2f GPI:%d\r\r", runTime, (bool)fParams[FEATURE_GPIO_MODE_INPUT_PARAM_STATUS]);
        fflush (stdout);  // update the output
        if ((bool)fParams[FEATURE_GPIO_MODE_INPUT_PARAM_STATUS] != bLastGpiValue)
        {
            bLastGpiValue = !bLastGpiValue;
            printf ("\n");
        }

        //
        // Step 5.2
        //      Wait the desired amount
        usleep (uPollPeriod*1000);
    }

cleanupAndExit:
    PxLUninitialize (hCamera);

	return rc;
}

void usage (char **argv)
{
        printf("\nTests/Demonstrates a custom PixeLINK camera that has been modified to accompdate a general\n");
        printf("purpose input signal.  Basically these cameas hae been modified to redirect the hardware \n");
        printf("trigger inut signal, to the first general purpose IO signal.\n\n");
        printf("    Usage: %s [-i] [-t poll_period] \n", argv[0]);
        printf("       where: \n");
        printf("          -i               Indicates that the input signal input should be inverted (by the camera) \n");
        printf("          -t poll_period   Wait poll_period milliseconds between each read of the input signal \n");
        printf("    Example: \n");
        printf("        %s -t 100 \n", argv[0]);
        printf("              Ths will poll the camera aproximatly 10 times a second, reporting on the General \n");
        printf("              Purpose Input signal).\n");
}

int getParameters (int argc, char* argv[], U32* pollPeriod, bool* invert)
{
    
    // Default our local copies to the user supplied values
    U32  uPollPeriod = *pollPeriod;
    bool bInvert = *invert;
   
    // 
    // Step 1
    //      Simple parameter parameter check
    if (argc < 1 || // No parameters specified -- OK
        argc > 4)
    {
        printf ("\nERROR -- Incorrect number of parameters");
        return GENERAL_ERROR;
    }
    
    //
    // Step 2
    //      Parse the command line looking for parameters.
    for (int i = 1; i < argc; i++)
    {
        if (!strcmp(argv[i],"-i") ||
            !strcmp(argv[i],"-I"))
        {
            bInvert = true;
        } else if (!strcmp(argv[i],"-t") ||
                   !strcmp(argv[i],"-T")) {
            if (i+1 >= argc) return GENERAL_ERROR;
            uPollPeriod = atoi(argv[i+1]);
            i++;
        } else {
            return GENERAL_ERROR;
        }
    }
    
    //
    // Step 3
    //      Let the app know the user parameters.
    *pollPeriod = uPollPeriod;
    *invert = bInvert;
    return A_OK;
}


