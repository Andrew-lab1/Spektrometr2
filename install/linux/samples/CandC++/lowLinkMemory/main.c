//
// lowLinkMemory/main.c
//
// Demonstrates how to detect / deal with systems configured with a small
// amount of link (USB or NetworkStack) memory.
//
// As per the INSTALL.INSTRUCTIONS.txt file that accompanies the PixeLINK SDK,
// by default, many Linux systems may not be configured with enough lin k buffer
// memory to permit optimal streaming for a high performancecamera, such
// as many models of the PixeLINK cameras.  This same TXT file offers advice on
// how you may reconfigure your system to allocate more USB buffer memory.
//
// However, should you choose not to re-configure your system, you can use the
// PixeLINK API function 'PxLSetStreamState' to determine if your system's
// USB memory requirements are less than optimal.  In particular, if the
// PxLSetStreamState returns a response code of 'ApiSucessLowMemory' when you
// attempt a START_STREAM or PAUSE_STREAM operation, then the PixeLINK API has
// detected that your systems link memory allocation is sub-optimal.  The
// PixeLINK API has made some internal 'concessions' to accomodate.  These
// concesions MAY result in some frame loss when the camera is streaming.  More
// on this below.
//
// So, what should your application do when PxLSetStreamState returns
// 'ApiSuccessLowMemory'??
// There are several otions:
//   1. Do nothing.
//        Meaning, the PixeLINK API has already made some internal concessions
//        to deal with the sub-optimal link buffer space.  If while streaming,
//        your camera sontinually flashes its LED green -- then no frame loss
//        os occuring.  However, if you see a periodic red flash from the the
//        camera, then you are experiencing some frame loss that is probably
//        the result of these concessions.  In which case, you may want to
//        consider one of the other options
//  2. Use a 8-bit pixel format (if you're not already).
//        The 12 and 16-bit pixel formats (MONO16, MONO12_PACKED, and BAYER16)
//        produce larger images that require more USB buffer memory
//  3. Reduce the region of interest (ROI) to reduce image size.
//  4. Use Pixel Addressing to reduce image size.
//  5. Reduce the frame rate.
//
// This sample, uses strategy #5 from above.

//
//
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <PixeLINKApi.h>

ULONG
getMinFrameRate (HANDLE hCamera, float *minValue)
{
    ULONG  rc;
    ULONG  bufferSize = 0;

    rc = PxLGetCameraFeatures(hCamera, FEATURE_FRAME_RATE, NULL, &bufferSize);
    if (API_SUCCESS(rc)) {
        CAMERA_FEATURES* pFeatureInfo = (CAMERA_FEATURES*)malloc(bufferSize);
        if (NULL != pFeatureInfo) {
            // Now read the information into the buffer
            rc = PxLGetCameraFeatures(hCamera, FEATURE_FRAME_RATE, pFeatureInfo, &bufferSize);
            if (API_SUCCESS(rc)) {
                *minValue = pFeatureInfo->pFeatures->pParams->fMinValue;
            }
            free(pFeatureInfo);
        }
    }

    return rc;
}

int 
main(int argc, char* argv[])
{
	HANDLE hCamera;
	ULONG  rc;

	float  currentFrameRate = 0.0f;
	float  minFrameRate;
	ULONG  numParams = 1;
	ULONG  flags;

	// We assume there's only one camera connected;
	rc = PxLInitialize(0, &hCamera);
	if (API_SUCCESS(rc)) {

        rc = getMinFrameRate (hCamera, &minFrameRate);
        if (API_SUCCESS(rc)) {
            rc = PxLGetFeature (hCamera, FEATURE_FRAME_RATE, &flags, &numParams, &currentFrameRate);
        }
        if (!API_SUCCESS(rc)) {
            printf ("Difficuty accessing FEATURE_FRAME_RATE, rc=0x%x\n", rc);
            PxLUninitialize(hCamera);
            return 1;
        }

        rc = PxLSetStreamState (hCamera, START_STREAM);
        if (!API_SUCCESS(rc)) {
            printf ("Difficuty starting the, rc=0x%x\n", rc);
            PxLUninitialize(hCamera);
            return 1;
        }

        while (ApiSuccessLowMemory == rc) {
            printf ("Sub-optimal USB memory allocation detected at a frame rate of %5.2f fps\n", currentFrameRate);
            if (currentFrameRate == minFrameRate) break; // We cannot reduce the frame rate any lower.

            // try restarting the stream at a lower frame rate
            rc = PxLSetStreamState (hCamera, STOP_STREAM);
            if (! API_SUCCESS(rc)) break;

            // reduce the frame rate by 20%, being careful to not go lower than the minimum value
            currentFrameRate = (currentFrameRate * 0.8) < minFrameRate ? minFrameRate : (currentFrameRate * 0.8);
            rc = PxLSetFeature (hCamera, FEATURE_FRAME_RATE, FEATURE_FLAG_MANUAL, numParams, &currentFrameRate);
            if (! API_SUCCESS(rc)) break;

            rc = PxLSetStreamState (hCamera, START_STREAM);
            if (! API_SUCCESS(rc)) break;
        }

        if (API_SUCCESS(rc))
        {
            if (ApiSuccessLowMemory != rc) {
                printf ("Camera can stream fine at a frame rate of %5.2f fps\n", currentFrameRate);
            } else {
                printf ("Cannot fully accomodate the stream because of sub-optimal link memory allocations. \n"
                        "If this is a GigE camera, increase the amount of receive buffer (as per INSTALL.INSTRUCTIONS.txt). \n"
                        "Otherwise, try adjusting ROI, PixelAddressing, or PixelFormat\n");
            }
        } else {
            printf ("Difficuty attempting to reduce the frame rate, rc=0x%x\n", rc);
	    }

        PxLSetStreamState (hCamera, STOP_STREAM);
        PxLUninitialize(hCamera);
	}
	return 0;
}

