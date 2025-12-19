//
// autoWhiteBalance.cpp 
//
// A simple little application to perfrom an auto white balance.
//
// Note1: This program does a 'directed' auto white balance.  That is, it uses
// the FEATURE_AUTO_ROI to provide guiadance to the auto white balance algorithm, 
// where to find 'white' in the image.  If this feature is not used, then the 
// camera will search the entire image, lookng for what it believes to be 'white'.
//
// Note2: The sample application 'whiteBalance' does not use the FEATURE_AUTO_ROI.
// 
// Note3: The sample applicaiton 'autoExposure' shows how to cancel an auto 
// operation.
//
//

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <vector>
#include <unistd.h>
#include <PixeLINKApi.h>

#define WAIT_SECONDS 10 //Wait this amount of time for a white balance to complete.

//simple function to abort a auto white balance
void abortAutoWhiteBalance (HANDLE hCamera)
{
    PXL_RETURN_CODE rc;
    float  rgbGains[3] = {1.0, 1.0, 1.0}; // We need to set them to something -- unity is a nice simple value
    
    rc = PxLSetFeature (hCamera, FEATURE_WHITE_SHADING, FEATURE_FLAG_MANUAL, 3, rgbGains);
    if (! API_SUCCESS (rc))
    {
        printf ("  Error - PxLSetFeature to cancel AutoWB returned 0x%x\n", rc);
    }
}


int main(const int argc, char const * const argv[])
{
    HANDLE hCamera = NULL;
    PXL_RETURN_CODE rc;
    float           params[5];
    ULONG           flags;
    ULONG           numParams;
    int             i;

    //Step 0 - environment stuff
    setbuf (stdout, NULL);  // Don't buffer std, output immediatley.
    
    // Step 1 - Grab a camera.
    rc = PxLInitialize(0, &hCamera);
    if (! API_SUCCESS(rc))
    {
        printf ("  Error - PxLInitialize returned 0x%x\n", rc);
        return 1;
    }
    
    // Step 2 - Get the current ROI
    numParams = 4;
    rc = PxLGetFeature (hCamera, FEATURE_ROI, &flags, &numParams, params);
    if (! API_SUCCESS(rc))
    {
        printf ("  Error - PxLGetFeature(FEATURE_ROI) returned 0x%x\n", rc);
        PxLUninitialize (hCamera);
        return 1;
    }
    
    // Step 3 - Set the AUTO_ROI to a 256x256 window in the centre of the ROI
    params[0] = (params[2] - 256.0)/2.0;
    params[1] = (params[3] - 256.0)/2.0;
    params[2] = params[3] = 256.0;
    rc = PxLSetFeature (hCamera, FEATURE_AUTO_ROI, FEATURE_FLAG_MANUAL, numParams, params);
    if (! API_SUCCESS(rc))
    {
        printf ("  Error - PxLSetFeature(FEATURE_AUTO_ROI) returned 0x%x\n", rc);
        PxLUninitialize (hCamera);
        return 1;
    }

    // Step 4 - Perform a one-time, auto white balance
    params[0] = params[1] = params[2] = 1.0f;
    rc = PxLSetFeature (hCamera, FEATURE_WHITE_SHADING, FEATURE_FLAG_ONEPUSH, 3, params);
    if (! API_SUCCESS(rc))
    {
        printf ("  Error - PxLSetFeature(FEATURE_WHITE_SHADING) returned 0x%x\n", rc);
        PxLUninitialize (hCamera);
        return 1;
    }

    // Step 5 - Wait for the white balance to complete
    printf ("Waiting on White Balance to complete\n");
    for (i=0; i<WAIT_SECONDS; i++)
    {
        rc = PxLGetFeature (hCamera, FEATURE_WHITE_SHADING, &flags, &numParams, params);
        if (!(flags & FEATURE_FLAG_ONEPUSH)) break; 
        printf("  Interim balance --> R:%f, G:%f B:%f\n", params[0], params[1], params[2]);
        usleep(1* 1000* 1000);
    }
    if (! API_SUCCESS(rc))
    {
        printf ("Error - PxLGetFeature(FEATURE_WHITE_SHADING) returned 0x%x\n", rc);
        abortAutoWhiteBalance(hCamera);
        PxLUninitialize (hCamera);
        return 1;
    }
    
    // The auto white balance completed successfully or with a warning -- or we got tired of wating.
    if (i==WAIT_SECONDS)
    {
        printf ("Tired of waiting on the white balance, aborting it\n");    
        abortAutoWhiteBalance(hCamera);
    } else {
        printf ("Final balance rc=0x%x --> R:%f, G:%f B:%f\n",rc, params[0], params[1], params[2]);
    }

    
    // Step 6 - Cleanup
    PxLUninitialize (hCamera);

    return 0;
}



