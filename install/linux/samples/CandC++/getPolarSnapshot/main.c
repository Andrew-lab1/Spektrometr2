//
// main.c
//
// Sample code to capture and save 4 encoded BMP images from a Pixelink polar camera; 
// where each image represents each of the 4 polar channels.
// 
//

#include <stdio.h>
#include "getPolarSnapshot.h"

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
    char* pEnding[] = { "0deg.bmp", "45deg.bmp", "90deg.bmp", "135deg.bmp" };
    char* pFileName = "";
    U32 polarChannel;
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
    
    // Check whether the polar camera is connected.
    if (!IsPolarCamera(hCamera)) 
    {
        PxLUninitialize(hCamera);
        printf("This is not a polar camera.\n");
        return FAILURE;
    }

    // Set camera pixel format to Polar4_12
    if (SetPolarPixelFormat(hCamera) == FAILURE)
    {
        PxLUninitialize(hCamera);
        return FAILURE;
    }

    for (polarChannel = 0; polarChannel < 4; polarChannel++)
    {
        // Prepare file name for each snapshot
        pFileName = PrepareFileName(pFileRoot, pEnding[polarChannel]);
        // Get a snapshot of each polar channel set, where one polar channel has a weighting of 100%, and the others are 0.
        // Save each image to a file
        retVal = GetPolarSnapshot(hCamera, IMAGE_FORMAT_BMP, pFileName, polarChannel);
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
    
    // Tell the camera we're done with it.
    if (!API_SUCCESS(PxLUninitialize(hCamera)))
    {
        return FAILURE;
    }

    return (SUCCESS == retVal) ? SUCCESS : FAILURE;
}
