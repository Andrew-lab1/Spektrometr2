//
// main.c
//
// Sample code to capture images from an HDR Pixelink camera and save 
// each encoded image to a file.
//

#include <stdio.h>
#include "getHDRSnapshot.h"

//
// If the user specifies a name on the command line, we'll use 
// that as the filename, otherwise use a default name.
//
enum {
    PARM_EXE_NAME = 0,
    PARM_FILENAME
};

int 
main(int argc, char* argv[])
{
    HANDLE hCamera;
    char* pFileRoot = "snapshot";
    char* pEnding[] = { "CameraHdr.bmp", "InterleavedHdr.bmp" };
    char* pFileName = "";
    U32 hdrMode, fileNameSelect;
    int retVal;

    // Did the user specify a filename to use?
    if (argc > PARM_FILENAME) 
    {
        pFileRoot = argv[PARM_FILENAME];
    }

    // Tell the camera we want to start using it.
    // NOTE: We're assuming there's only one camera.
    if (!API_SUCCESS(PxLInitialize(0, &hCamera))) 
    {
        return FAILURE;
    }

    // Check whether the connected camera is an HDR camera.
    if (!DoesCameraSupportHdr(hCamera))
    {
        PxLUninitialize(hCamera);
        printf("This is not an HDR camera.\n");
        return FAILURE;
    }

    // Get a snapshot for each gain HDR mode
    for (fileNameSelect = 0, hdrMode = FEATURE_GAIN_HDR_MODE_CAMERA;
         hdrMode <= FEATURE_GAIN_HDR_MODE_INTERLEAVED;
         fileNameSelect++, hdrMode++)
    {
        // Set gain HDR mode to "Camera HDR" or "Interleaved HDR"
        if (SetHdrMode(hCamera, hdrMode) == FAILURE)
        {
            PxLUninitialize(hCamera);
            return FAILURE;
        }
        
        // Prepare a snapshot file name for each HDR mode
        pFileName = PrepareFileName(pFileRoot, pEnding[fileNameSelect]);

        // Get a snapshot for each gian HDR mode and save it to a file
        retVal = GetHdrSnapshot(hCamera, IMAGE_FORMAT_BMP, pFileName);
        if (SUCCESS == retVal)
        {
            printf("Saved image to '%s'\n", pFileName);
        }
        else
        {
            printf("ERROR: Unable to capture an image\n");
            free(pFileName);
            break;
        }

        free(pFileName);
    }

    // Don't leave HDR on
    SetHdrMode(hCamera, FEATURE_GAIN_HDR_MODE_NONE);

      
    // Tell the camera we're done with it.
    if (!API_SUCCESS(PxLUninitialize(hCamera))) 
    {
        return FAILURE;
    }

    return (SUCCESS == retVal) ? SUCCESS : FAILURE;
}
