//
// multiCameraPreview.c
//
// Sample code to show a simple image preview.  Furthermore, this particular program
// will work with multiple cameras connected, allowing the user to choose which 
// camera(s) to preview.
//
#include "PixeLINKApi.h"
#include "LinuxUtil.h"

#include <iostream>
#include <stdio.h>
#include <stdexcept>
#include <unistd.h>
#include <vector>

using namespace std;

typedef struct
{
   ULONG  serialNum;
   HANDLE hCamera;
   bool   previewing;
   HWND   previewWindow;
} CameraInfo, *PCameraInfo;

int main() {
    PXL_RETURN_CODE rc;
    ULONG numCameras = 0;
 
    //
    // Step 1
    //    Get information on all connected cameras
    rc = PxLGetNumberCamerasEx(NULL, &numCameras);
    if (!API_SUCCESS(rc))
    {
        printf ("Could not even enumerate any cameras!  Rc = 0x%X\n", rc);
        return 1;
    }

    vector<CAMERA_ID_INFO> connectedCameras(numCameras);
    // Let the PixeLINK API know the size of CAMERA_ID_INFO we are using
    if (numCameras) connectedCameras[0].StructSize = sizeof (CAMERA_ID_INFO);
    rc = PxLGetNumberCamerasEx(&connectedCameras[0], &numCameras);
    if (!API_SUCCESS(rc))
    {
        printf ("Could not even enumerate the cameras!  Rc = 0x%X\n", rc);
        return 1;
    }

    //
    // Step 2
    //    Show the camera info for all available cameras
    printf ("Camera #    Serial #      \n");
    for (ULONG i = 0; i < numCameras; i++)
    {
	printf ("   %2d      %d    \n",
                 i+1, connectedCameras[i].CameraSerialNum);
    }
    if (numCameras < 1)
    {
	// No point in continuing
        printf ("No cameras found.\n");
        return 0; // Not really and error, so return 0
    }

    //
    // Step 3
    //    Record all of the camera 'state' information
    vector<CameraInfo> myCameras(numCameras);
    for (ULONG i = 0; i < numCameras; i++)
    {
         myCameras[i].serialNum = connectedCameras[i].CameraSerialNum;
         rc = PxLInitializeEx(myCameras[i].serialNum,&(myCameras[i].hCamera),0);
         myCameras[i].previewing = false;
         if (!API_SUCCESS(rc))
         {
             printf ("Could not initialize camera %d (serial number:%d)\n",
                     i+1, myCameras[i].serialNum);
             // We may have sucessfully initialized some cameras.  but, since we
             // are about to exit -- I'll dispense with the uninitialize.
             return 1;
         }
    }

    //
    // Step 4
    //    Toggle the preview according to the users wishes
    DontWaitForEnter unbufferedKeyboard;  // Declare this for our getchar
    int   keyPressed;
    bool  done = false;
    HWND  previewWin;
    printf ("Press a number key to toggle that cameras preview.  Q to quit\n");
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
                done = true;
                break;
            case '1': case '2': case '3': case '4': case '5':
            case '6': case '7': case '8': case '9': 
                ULONG cameraNum = (keyPressed-0x30)-1; // Assumes ASCII character set.
                if (cameraNum >= numCameras) break;  // Ignore numbers not mapped to a camera
                if (!myCameras[cameraNum].previewing)
                {
                    rc = PxLSetStreamState (myCameras[cameraNum].hCamera, START_STREAM);
                    if (API_SUCCESS(rc))
                    {
                        rc = PxLSetPreviewState (myCameras[cameraNum].hCamera, 
                                                 START_PREVIEW, 
                                                 &(myCameras[cameraNum].previewWindow));
                    }
                } else {
                    rc = PxLSetPreviewState (myCameras[cameraNum].hCamera, 
                                             STOP_PREVIEW, 
                                             &(myCameras[cameraNum].previewWindow));
                    if (API_SUCCESS(rc))
                    {
                        rc = PxLSetStreamState (myCameras[cameraNum].hCamera, STOP_STREAM);
                    }
                }
                myCameras[cameraNum].previewing = ! myCameras[cameraNum].previewing;
                if (!API_SUCCESS(rc))
                {
                    printf ("Difficulties setting preview state, rc = 0x%x\n", rc);
                    done = true;
                }
                break;
            }
            fflush(stdout);
        }
        usleep (50*1000);  // 50 ms - don't hog all of the CPU
    }

    //
    // Step 5
    //     Clean up
    for (ULONG i = 0; i < numCameras; i++)
    {
         if (myCameras[i].previewing)
         {
             PxLSetPreviewState (myCameras[i].hCamera, STOP_PREVIEW, &previewWin);
             PxLSetStreamState (myCameras[i].hCamera, STOP_STREAM);
         }
         PxLUninitialize(myCameras[i].hCamera);
     }

    return 0;
}

