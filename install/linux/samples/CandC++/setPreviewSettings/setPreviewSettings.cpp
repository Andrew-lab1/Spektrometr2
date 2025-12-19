//
// setPreviewSettings.cpp 
//
// A simple application that previews the camera, and demonstrates how
// to make adjustments to that window, as well as some camera settings
// that affect the camera's preview.
//
//

#include <PixeLINKApi.h>
#include <cassert>
#include <stdio.h>
#include <unistd.h>
#include <vector>
#include "LinuxUtil.h"

#define ASSERT(x)	do { assert((x)); } while(0)

/**
 * Class:   CInterruptStreamState
 * Purpose: Certain camera operations cannot be done while the camera is streaming
 *          A varaible of this class can be declared to temporarily stop the video stream
 *          within the scope of a code block.
 */
class CInterruptStreamState
{
public:
    CInterruptStreamState(HANDLE hCam)
: m_hCam (hCam)
{
        HWND hPreview;
        ASSERT(API_SUCCESS(PxLSetPreviewState(m_hCam, STOP_PREVIEW, &hPreview)));
        ASSERT(API_SUCCESS(PxLSetStreamState(m_hCam, STOP_STREAM)));
}
    ~CInterruptStreamState()
    {
        HWND hPreview;
        ASSERT(API_SUCCESS(PxLSetStreamState(m_hCam, START_STREAM)));
        ASSERT(API_SUCCESS(PxLSetPreviewState(m_hCam, START_PREVIEW, &hPreview)));
    }
private:
    HANDLE	m_hCam;
};


int 
main(int argc, char* argv[])
{
    HANDLE 		hCamera;
    HWND   		previewHandle;
    PXL_RETURN_CODE rc;
    bool        bColorCamera = true;

    //
    // Step 0
    // 	Grab the first camera we find
    rc = PxLInitializeEx(0, &hCamera, 0);
    if (API_SUCCESS(rc)) {

        //
        // Step 1.
        //      Check to make sure we are using the 'standard' API
        if (ApiSuccessApiLiteWarning == rc)
        {
            printf ("ERROR: Api Lite detected -- this application requires the standard Pixelink API\n");
            PxLUninitialize(hCamera);
        }
        
        //
        // Step 2.
        //     Preview at a set ROI of 800 x 600

        // Set the ROI to a fixed size of 800x600
        std::vector<float> roi (4, 0.0);
        roi[2] = 800.0; roi[3] = 600.0;
        rc = PxLSetFeature (hCamera, FEATURE_ROI, FEATURE_FLAG_MANUAL, 4, &roi[0]);
        ASSERT(API_SUCCESS(rc));

        // Just use all of the other camera's default settings.
        // Start the stream
        rc = PxLSetStreamState(hCamera, START_STREAM);
        ASSERT(API_SUCCESS(rc));

        printf ("Previewing 800 x 600...\n");

        // Start the preview (NOTE: camera must be streaming)
        rc = PxLSetPreviewState(hCamera, START_PREVIEW, &previewHandle);
        ASSERT(API_SUCCESS(rc));

        sleep (5); //delay 5 seconds

        //
        // Step 3
        //     Change the preview window to 1024 x 768

        printf ("Scaling preview up to 1024 x 768...\n");

        rc = PxLSetPreviewSettings (hCamera, "1024 x 768 preview window", 0, 0, 0, 1024, 768);
        ASSERT(API_SUCCESS(rc));

        sleep (5); // delay 5 seconds

        //
        // Step 4
        //     Change to the ROI to 640 x 480

        printf ("Changing ROI to 640 x 480, preview window size is the same, but the image is zoomed in...\n");


        // The ROI cannot be chaned while streaming, so interrupt the stream for the adjustment
        {
            CInterruptStreamState interruptStream(hCamera);

            roi[2] = 640.0; roi[3] = 480.0;
            rc = PxLSetFeature (hCamera, FEATURE_ROI, FEATURE_FLAG_MANUAL, 4, &roi[0]);
            ASSERT(API_SUCCESS(rc));
        }

        sleep (5); // delay 5 seconds

        //
        // Step 5
        //     Change to the ROI to 1280 x 1024

        printf ("Changing ROI to 1280 x 1024, preview window size is the same, but the image is zoomed out...\n");


        // The ROI cannot be chaned while streaming, so interrupt the stream for the adjustment
        {
            CInterruptStreamState interruptStream(hCamera);

            roi[2] = 1280; roi[3] = 1024.0;
            rc = PxLSetFeature (hCamera, FEATURE_ROI, FEATURE_FLAG_MANUAL, 4, &roi[0]);
            ASSERT(API_SUCCESS(rc));
        }

        sleep (5); // delay 5 seconds

        //
        // Step 6
        //     Change to YUV format (if supported)

        // Stop the preview
        //rc = PxLSetPreviewState(hCamera, STOP_PREVIEW, &previewHandle);
        //ASSERT(API_SUCCESS(rc));
        printf ("Changing pixel format to YUV...\n");


        // The pixel format cannot be chaned while streaming, so interrupt the stream for the adjustment
        {
            CInterruptStreamState interruptStream(hCamera);

            float pixelFormat = (float)PIXEL_FORMAT_YUV422;
            rc = PxLSetFeature (hCamera, FEATURE_PIXEL_FORMAT, FEATURE_FLAG_MANUAL, 1, &pixelFormat);
            if (! API_SUCCESS(rc))
            {
                printf ("Changing pixel format to YUV returned 0x%x\n", rc);
                bColorCamera = false;
            }
        }

        // Start the preview
        //rc = PxLSetPreviewState(hCamera, START_PREVIEW, &previewHandle);
        //ASSERT(API_SUCCESS(rc));
        sleep (5); // delay 5 seconds

        //
        // Step 7
        //     Change to 16bit format

        printf ("Changing pixel format to 16 bit...\n");


        // The pixel format cannot be chaned while streaming, so interrupt the stream for the adjustment
        {
            CInterruptStreamState interruptStream(hCamera);

            float pixelFormat = (float)(bColorCamera ? PIXEL_FORMAT_BAYER16 : PIXEL_FORMAT_MONO16);
            rc = PxLSetFeature (hCamera, FEATURE_PIXEL_FORMAT, FEATURE_FLAG_MANUAL, 1, &pixelFormat);
            ASSERT(API_SUCCESS(rc));
        }

        sleep (5); // delay 5 seconds

        //
        // Step 8
        //     Change the window size to match the ROI

        printf ("Changing preview window size to match the ROI (1280 x 1024)...\n");

        rc = PxLResetPreviewWindow (hCamera);
        ASSERT(API_SUCCESS(rc));
        sleep (5); // delay 5 seconds

        //
        // Step 9
        //     Change back to simple 8bit format

        printf ("Changing pixel format to 8 bit...\n");

        // The pixel format cannot be chaned while streaming, so interrupt the stream for the adjustment
        {
            CInterruptStreamState interruptStream(hCamera);

            float pixelFormat = (float)(bColorCamera ? PIXEL_FORMAT_BAYER8 : PIXEL_FORMAT_MONO8);
            rc = PxLSetFeature (hCamera, FEATURE_PIXEL_FORMAT, FEATURE_FLAG_MANUAL, 1, &pixelFormat);
            ASSERT(API_SUCCESS(rc));
        }

        sleep (5); // delay 5 seconds

        //
        // Step 10
        //     Vertically flip the image

        printf ("Vertically flipping the image...\n");

        // The FEATURE_FLIP cannot be chaned while streaming, so interrupt the stream for the adjustment
        {
            CInterruptStreamState interruptStream(hCamera);

            std::vector<float> flip (2,0.0);
            flip[0] = 0.0f; flip[1] = 1.0f;
            rc = PxLSetFeature (hCamera, FEATURE_FLIP, FEATURE_FLAG_MANUAL, 2, &flip[0]);
            ASSERT(API_SUCCESS(rc));
        }

        sleep (5); // delay 5 seconds

        //
        // Step 11
        //     Rotate the image 270 degrees

        printf ("Rotating the image...\n");

        // The FEATURE_ROTATE cannot be chaned while streaming, so interrupt the stream for the adjustment
        {
            CInterruptStreamState interruptStream(hCamera);

            float rotate = 270.0f;
            rc = PxLSetFeature (hCamera, FEATURE_ROTATE, FEATURE_FLAG_MANUAL, 1, &rotate);
            ASSERT(API_SUCCESS(rc));
        }

        sleep (5); // delay 5 seconds

        //
        // Step 12
        //     Done.  Just tidy up.


        // Stop the preview
        rc = PxLSetPreviewState(hCamera, STOP_PREVIEW, &previewHandle);
        ASSERT(API_SUCCESS(rc));

        // Stop the stream
        rc = PxLSetStreamState(hCamera, STOP_STREAM);
        ASSERT(API_SUCCESS(rc));

        // Uninitialize the camera now that we're done with it.
        PxLUninitialize(hCamera);
    }
    return 0;
}



