//
// measureGetNextFrameRate.c
//
// Sample code to show a very simple frame grab.  See the sample application
// 'getNextFrame' for a more robust frame grab example.  
//
// This program simply calculates the frame rate of the camera (via frame grabs).
//
#include "PixeLINKApi.h"
#include "LinuxUtil.h"

#include <iostream>
#include <stdio.h>
#include <stdexcept>
#include <unistd.h>
#include <vector>

using namespace std;

#define NO_CAMERA(rc) ((rc == ApiNoCameraError) || (rc == ApiNoCameraAvailableError))

int main() {
    HANDLE myCamera;
    U32    frameCount, badFrameCount;
    U32    getNextFramesPerItteration = 1;
    time_t  startTime, currentTime;

    DontWaitForEnter unbufferedKeyboard;  // Declare this for our getchar

    PXL_RETURN_CODE rc;

    // Just going to declare a very large buffer here
    // One that's large enough for any PixeLINK 4.0 camera
    std::vector<U8> frameBuffer(3000*3000*2);
    FRAME_DESC frameDesc;
    frameDesc.uSize = sizeof(FRAME_DESC);

    fflush(stdin);

    rc = PxLInitializeEx(0,&myCamera,0);
    if (!API_SUCCESS(rc))
    {
        printf ("Could not Initialize the camera!  Rc = 0x%X\n", rc);
        return 1;
    }

    rc = PxLSetStreamState(myCamera, START_STREAM);
    if (!API_SUCCESS(rc))
    {
        printf ("Could not stream the camera!  Rc = 0x%X\n", rc);
        PxLUninitialize (myCamera);
        return 1;
    }

    printf("Counting the number of images over a 20 second period...\n");

    frameCount = badFrameCount =  0; 
    startTime = currentTime = timeInMilliseconds();

    while (1) {
       
      for (U32 i=0; i<getNextFramesPerItteration; i++) {
           rc = PxLGetNextFrame(myCamera, (U32)frameBuffer.size(), &frameBuffer[0], &frameDesc);
           if (API_SUCCESS(rc)) {
               frameCount++;
           } else {
               badFrameCount++;
               if (NO_CAMERA(rc)) 
               {
                   printf ("Camera is Gone!! -- Aborting\n");
                   return 1;  // No point is continuing
               }
               break; // Do a time check to see if we are done.
           }
      }
       
      currentTime = timeInMilliseconds();

      if (currentTime >=  startTime + 20000) break;

      if (currentTime <= startTime + 200) getNextFramesPerItteration = getNextFramesPerItteration << 1;
    }
                  
     
    PxLSetStreamState(myCamera, STOP_STREAM);

    printf ("    Received %d frames (%d bad), or %8.2f frames/second.  %d PxLGetNextFrames/timecheck\n", 
            frameCount+badFrameCount, badFrameCount, (float)(frameCount+badFrameCount)/((float)(currentTime - startTime) / 1000.0f), 
            getNextFramesPerItteration);

    printf ("Press any key to exit\n");            
    getchar();
    
    PxLUninitialize (myCamera);

    return 0;
}

