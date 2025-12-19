/*
 * ptpV2.c
 *
 * Demonstration of a trivial interaction with the PixeLINK API
 *
 * More specifically, this demo will demonstarte how to use IEEE 1588 (PTPv2)
 * on a Pixelink camera.  This demo program has minimal error handling, as 
 * its purpose is  to show minimal code to interact with the PixeLINK API, not
 * tell you how to do your error handling.
 *
 * With this program, we assume that there is at least one camera
 * connected, and that no cameras are connected or disconnected 
 * while the program is running.
*/
#include "PixeLINKApi.h"

#include <unistd.h>
#include <stdio.h>
#include <vector>

#define EXIT_SUCCESS    0
#define EXIT_FAILURE    1


bool transitoryPtpState (U32 state)
{
    // Returns true if the PTP state machine is in a transitory (non-stable) state
    switch (state)
    {
        case FEATURE_PTP_STATUS_INITIALIZING: return true;
        case FEATURE_PTP_STATUS_FAULTY:       return false;
        case FEATURE_PTP_STATUS_DISABLED:     return false;
        case FEATURE_PTP_STATUS_LISTENING:    return true;
        case FEATURE_PTP_STATUS_PREMASTER:    return true;
        case FEATURE_PTP_STATUS_MASTER:       return false;
        case FEATURE_PTP_STATUS_PASSIVE:      return false;
        case FEATURE_PTP_STATUS_UNCALIBRATED: return true;
        case FEATURE_PTP_STATUS_SLAVE:        return false;
        default:                              return false;
    }

    return false;
}

int main(const int argc, char const * const argv[])
{
	// Initialize any camera
	HANDLE hCamera;
	PXL_RETURN_CODE rc = PxLInitialize(0, &hCamera);
	if (!API_SUCCESS(rc)) {
        printf("Error: Unable to initialize a camera: rc=%08X\n", rc);
		  return EXIT_FAILURE;
	}

    // do a get on the feature, mostly to make sure the camera supports it.
    U32 flags;
    U32 numParams = FEATURE_PTP_NUM_PARAMS;
    float ptpParams[FEATURE_PTP_NUM_PARAMS];
    rc = PxLGetFeature (hCamera, FEATURE_PTP, &flags, &numParams, ptpParams);
	if (!API_SUCCESS(rc)) {
        printf("Error: Unable to get FEATURE_PTP for the camera: rc=%08X\n", rc);
        PxLUninitialize (hCamera);
		  return EXIT_FAILURE;
	}

    printf ("Enabling PTP Auto mode, and waiting for clock synchronization...\n");
    ptpParams[FEATURE_PTP_PARAM_MODE] = FEATURE_PTP_MODE_AUTOMATIC;
    rc = PxLSetFeature (hCamera, FEATURE_PTP, FEATURE_FLAG_AUTO, numParams, ptpParams);
	if (!API_SUCCESS(rc)) {
        printf("Error: Unable to set FEATURE_PTP for the camera: rc=%08X\n", rc);
        PxLUninitialize (hCamera);
		  return EXIT_FAILURE;
	}
    while (true)
    {
        rc = PxLGetFeature (hCamera, FEATURE_PTP, &flags, &numParams, ptpParams);
	    if (!API_SUCCESS(rc)) {
            printf("Error: Unable to get FEATURE_PTP for the camera: rc=%08X\n", rc);
            PxLUninitialize (hCamera);
		      return EXIT_FAILURE;
    	}
        printf("    \r Current PTP state:%d", (U32)ptpParams[FEATURE_PTP_PARAM_STATUS]);
        if (!transitoryPtpState ((U32)ptpParams[FEATURE_PTP_PARAM_STATUS])) break;
        usleep (250*1000); // Take a breather
    }

    printf ("\n PTP Info:\n");
    printf ("    Mode: %d\n", (U32)ptpParams[0]);
    printf ("    Status: %d\n", (U32)ptpParams[1]);
    printf ("    My Clock Accuracy: %.9f\n", ptpParams[2]);
    printf ("    Offset From Master: %.9f\n", ptpParams[3]);
	
    PxLUninitialize (hCamera);
    return EXIT_SUCCESS;
}

