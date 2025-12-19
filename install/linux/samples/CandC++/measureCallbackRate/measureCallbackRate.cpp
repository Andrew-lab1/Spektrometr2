//
// measureCallbackRate.c
//
// Sample code to show how to create a simple callback.  This program simply
// calculates the frame rate of the camera (via callbacks).
//
#include "PixeLINKApi.h"
#include "LinuxUtil.h"

#include <iostream>
#include <stdio.h>
#include <stdexcept>
#include <unistd.h>
#include <vector>

using namespace std;

static U32 s_callbackCount;  // Careful -- this static is not protected by a mutex

static U32 SimpleCallback(
	HANDLE		hCamera,
	LPVOID		pData,
	U32			dataFormat,
	FRAME_DESC const *	pFrameDesc,
	LPVOID		userData)
{
    s_callbackCount++;
    return ApiSuccess;
}

int main() {
    HANDLE myCamera;
    U32    callbackCount;

    DontWaitForEnter unbufferedKeyboard;  // Declare this for our getchar

    ULONG rc;

    fflush(stdin);

    rc = PxLInitializeEx(0,&myCamera,0);
    if (!API_SUCCESS(rc))
    {
        printf ("Could not Initialize the camera!  Rc = 0x%X\n", rc);
        return 1;
    }

    if (API_SUCCESS (PxLSetCallback (myCamera, CALLBACK_FRAME, NULL, SimpleCallback))) {
        if (API_SUCCESS (PxLSetStreamState(myCamera, START_STREAM))) {

            
            printf("Counting the number of images over a 20 second period...\n");

            s_callbackCount = 0; 
            sleep(20); //Delay 20 seconds
            callbackCount = s_callbackCount;
            
            PxLSetStreamState(myCamera, STOP_STREAM);
            PxLSetCallback (myCamera, CALLBACK_FRAME, NULL, NULL); //Remove the callback
            printf ("    Received %d frames, or %8.2f frames/second\n", callbackCount, (float)callbackCount/20.0f);
            printf ("Press any key to exit\n");
            
            getchar();
        }
    }
    
    PxLUninitialize (myCamera);

    return 0;
}

