//
// Test program that simply grabs images from the camera (using PxLGetNextFrame) and writes them
// to disk.  It limits the file size, and aborts if the camera is streaming at a rate faster than
// we can write to disk.
//

#include <iostream>
#include <stdio.h>
#include <stdexcept>
#include <unistd.h>
#include <stdlib.h>
#include <cassert>
#include <vector>
#include <string.h>
#include "LinuxUtil.h"
#include "PixeLINKApi.h"

using namespace std;

//
// A few useful defines and enums.
//
#define ASSERT(x)	do { assert((x)); } while(0)
#define A_OK            0  // non-zero error codes
#define GENERAL_ERROR   1

#define BUFFER_SIZE (1 << 4)  // 16 Must be a power of 2
#define BUFFER_SIZE_MASK (BUFFER_SIZE - 1) 

// As a simplification, I'm going to use a constant buffer size.  A better / more
// spohisticated applicaiton, would check the camera's frame buffer size by getting
// the following camera features:
//     FEATURE_ROI
//     FEATURE_PIXEL_FORMAT
//     FEATURE_PIXEL_ADDRESSING
//     FEATURE_GAIN_HDR (for HDR cameras)
// When it comes to writing the image to disk, it will use the actual image size (but
// still with some assumptions / simplifications).
#define MAX_IMAGE_SIZE (25 * 1024 * 1024)  // 25 Megabytes.
static U32 s_frameSize = MAX_IMAGE_SIZE;

typedef enum _RUNTIME_STATUS
{
   STATUS_RUNNING = 0,  // Source and Sink are both running, and all is good
   STATUS_USER_STOPPED, // The user has requested the applicaiton stop
   STATUS_SINK_OVERRUN, // The source is getting frame faster than we can write them
                        //     to disk.  Either slow down the camera, or speed up the disk.
   STATUS_ERROR         // Some sort of error detected, requiring a shutdown
} RUNTIME_STATUS;

typedef struct _FRAME_DATA
{
   U8*          m_image;
   FRAME_DESC   m_descriptor;
} FRAME_DATA, *PFRAME_DATA;
//
// Some static (globals) for a pool of frame buffer queue that houses captured images.  The frame buffers are simply
// used in the pool in a round-robin fashion, and the frameQueue used to pass buffers from the source to the sink, 
// ensures that a fram buffer in the pool is not re-used until after we are done with it.
static FRAME_DATA**  s_frames;
// static U32   s_head;  // Not actually needed, as we uses a vector (queue) for the frame buffers passed beween
                         // the source and the sink.
static U32   s_tail;

// Queue of frame buffes being passed from the source to the sink.  A more sophisticated version of this application will
// use a semaphore to ensure that a frame buffer does not get used reeused before it's time.  Instead, this simple version
// simply monitors the number of entries in the queue, and if it's size is increasing and starts to approach the size of
// the buffer pool, a STATUS_SINK_OVERRUN error is returned.
static vector<FRAME_DATA*> s_frameQueue;

static HANDLE s_hCamera = NULL;
static U32    s_expectedFrameNumber;

static RUNTIME_STATUS s_status;

#define MAX_IMAGES (32) // the maximum number of images to put into the file (to prevent it from consuming the disk).
static char s_fileName[] = "./imageData.bin";


// Prototypes to allow top-down structure
void* sourceThread (void* param);
void* sinkThread (void* param);

int main (int argc, char* argv[])
{
    //
    // Step 1
    //      Allocate some frameBuffers and desriptors used to capture frames.
    s_frames      = new FRAME_DATA*[BUFFER_SIZE];
    int i;
    for (i=0; i<BUFFER_SIZE; i++)
    {
      s_frames[i]          = new FRAME_DATA;
      s_frames[i]->m_image = new U8[MAX_IMAGE_SIZE];
    }
    s_tail = 0;
    
    //
    // Step 2
    //      Initialize a camera
    s_hCamera = NULL;
    PXL_RETURN_CODE rc;
    CAMERA_ID_INFO cameraId;
    U32 numCameras = 0;
    rc = PxLGetNumberCamerasEx (&cameraId, &numCameras);
    if (!API_SUCCESS(rc) || numCameras != 1)
    {
      printf ("   Please ensure there is exactly one Pixelink camera connected\n");
      goto Cleanup;
    }
    
    rc = PxLInitializeEx (0, &s_hCamera, 0); 
    if (!API_SUCCESS(rc))
    {
      printf ("   Could not initialize the camera.  RC:0x%08X\n", rc);
      goto Cleanup;
    }
    
    // 
    // Step 3
    //      Start the stream
    rc = PxLSetStreamState (s_hCamera, START_STREAM);
    if (!API_SUCCESS(rc))
    {
      printf ("   Could not stream the camera.  RC:0x%08X\n", rc);
      goto Cleanup;
    }
    
    //
    // Step 4
    //      Start the source and sink threads
    s_status = STATUS_RUNNING;
    
    pthread_t source;
    pthread_t sink;
    pthread_create (&source, NULL, sourceThread, (void*)"SourceThread");
    pthread_create (&sink, NULL, sinkThread, (void*)"SinkThread");
    
    // Wait for a bit, just to be sure the threads started OK.
    usleep (500 * 1000);  // 500 milliseconds.
    if (s_status != STATUS_RUNNING) goto Cleanup;
    
    printf ("   Capturing image data, writing it to %s\n", s_fileName);
    printf ("        -- Press any key to stop --\n");
   	
    while (s_status == STATUS_RUNNING)
    {
      if (kbhit()) 
      {
         s_status = STATUS_USER_STOPPED;
         break;
      }
    }
    
    // Wait a little bit, just to be sure that both source and sink threads have had a chance to finish.
    // If the source thread was blocked on PxLGetNextFrame; this will unblock it (and you may see a ApiNoStreamError (0x90000005)
    if (s_hCamera) PxLSetStreamState (s_hCamera, STOP_STREAM); 
    usleep (1000 * 1000);  // 1000 milliseconds.

Cleanup:
    //
    // Step 5
    if (s_hCamera) PxLSetStreamState (s_hCamera, STOP_STREAM); // Belt and suspenders.      
    PxLUninitialize (s_hCamera);
    
    for (i=0; i<BUFFER_SIZE; i++)
    {
      delete[] s_frames[i]->m_image;
      delete[] s_frames[i];
    }
    delete []s_frames;
}

void* sourceThread (void* param)
{
   //
   // Step 1
   //    Grab a single image, just so that we can get the intial descriptor number, and the actual image size
    PXL_RETURN_CODE rc;

   FRAME_DATA* pFrame = s_frames[s_tail];
   rc = PxLGetNextFrame (s_hCamera, MAX_IMAGE_SIZE, &pFrame->m_image[0], &pFrame->m_descriptor);
   if (!API_SUCCESS(rc))
   {
      printf ("   Cound not get initial frame\n");
      s_status = STATUS_ERROR;
      goto Cleanup;
   }  
   
   s_expectedFrameNumber = pFrame->m_descriptor.uFrameNumber+1;
   // Now that we have a frame, figure out theexact size of each frame.  As a simpilification, we will assume 
   // only 1 byte pixel (MONO or BAYER8), no PixelAddressing or HDR
   s_frameSize = (U32) (pFrame->m_descriptor.Roi.fWidth * pFrame->m_descriptor.Roi.fHeight);
   
   //
   // Step 2
   //    Grab a frame with each loop itteration
   
   while (s_status == STATUS_RUNNING)
   {
      pFrame = s_frames[s_tail];
      s_tail = (s_tail + 1) & BUFFER_SIZE_MASK;
      rc = PxLGetNextFrame (s_hCamera, s_frameSize, &pFrame->m_image[0], &pFrame->m_descriptor); // Blocking call.
      if (!API_SUCCESS(rc))
      {
         // Opps, didn't actualyl get a frame, so re-use the buffer next time.
         printf ("   Could not get a frame.  RC:0x%08X\n", rc);
         s_tail = (s_tail - 1) & BUFFER_SIZE_MASK;
         continue;
      }
      
      //
      // Step 2.1
      //    Got a frame, see if we missed any
      if (pFrame->m_descriptor.uFrameNumber != s_expectedFrameNumber)
      {
         printf ("   Expected frame %d, but got frame %d\n", s_expectedFrameNumber, pFrame->m_descriptor.uFrameNumber);
      } 
      s_expectedFrameNumber = pFrame->m_descriptor.uFrameNumber+1;
      
      //
      // Step 2.2
      //    See if we are getting close to 'full'.  If so, stop with an error
      if (s_frameQueue.size () >= (BUFFER_SIZE - 3)) // 3 away from full capacity is getting too close for comfort
      {
         // The frame buffer queue is getting too large -- the sink can't keep up with the source
         printf ("   Error -- the Sink cannot keep up with the Source -- try slowing down the camera.\n");
         s_status = STATUS_SINK_OVERRUN; 
         break;
      }
      
      //
      // Step 2.3
      //    Add this frame to the frame queue
      s_frameQueue.push_back (pFrame);
   }
    
   
Cleanup:
   pthread_exit (NULL);
}
    

void* sinkThread (void* param)
{
   size_t numBytesWritten = 0;
   U32 numFramesInFile = 0;
   FRAME_DATA* pFrame;

   //
   // Step 1
   //    Create the file we will use for the image data.
   FILE* dataFile;
   dataFile = fopen(s_fileName, "wb");
   if (NULL == dataFile) 
   {
      printf (" Error:  Could not open/create the data file.\n");
      s_status = STATUS_ERROR;
      goto Cleanup;
   }
   
   //
   // Step 2
   //    Pull off frames from the frame queue, and write them to disk
   //
   //    Note that this tread does not block at all, so it will consume (effectivly) 100% of
   //    a core.  A more sophisticated approach would have this thread block when the queue
   //    is empty, wating on an event posted by the source when a new frame becomes available.
   while (s_status == STATUS_RUNNING)
   {
      if (!s_frameQueue.empty())
      {
         pFrame = s_frameQueue[0];
         s_frameQueue.erase (s_frameQueue.begin());
         
         //
         // Step 2.1
         //    Write the frame to disk
         numBytesWritten = fwrite(&pFrame->m_image[0], sizeof(U8), s_frameSize, dataFile);
         if (numBytesWritten != s_frameSize) 
         {
            printf (" Error:  Could not write to %d bytes to the data file.  Only wrote %d bytes\n", s_frameSize, (int)numBytesWritten);
            s_status = STATUS_ERROR;
            goto Cleanup;
         }
         
         //
         // Step 2.2
         //    Don't let the data file become too large.  If we have hit our limit, start over.
         if (++numFramesInFile >= MAX_IMAGES)
         {
            fseek (dataFile, 0, SEEK_SET);
            numFramesInFile = 0;
         }
      }
   }
      
Cleanup:
   fclose (dataFile);
   pthread_exit (NULL);
}
    

