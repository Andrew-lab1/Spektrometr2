// Sample code show how to receive, via callback, both compressed and decompressed
// images from a camera that has compression enabled.
//

#include <stdio.h>
#include <unistd.h>
#include <assert.h>
#include <vector>
#include <assert.h>
#include <PixeLINKApi.h>

using namespace std;

#define A_OK          0  // non-zero error codes
#define GENERAL_ERROR 1

#define ON  1
#define OFF 0

#define IS_BAYER8(pixelFormat) \
   ((pixelFormat == PIXEL_FORMAT_BAYER8_RGGB) || \
    (pixelFormat == PIXEL_FORMAT_BAYER8_GBRG) || \
    (pixelFormat == PIXEL_FORMAT_BAYER8_BGGR) || \
    (pixelFormat == PIXEL_FORMAT_BAYER8_GRBG))

typedef struct _COMPRESSED_WORKSPACE
{
   COMPRESSION_INFO_PIXELINK10 compressionInfo; // Required structure for CALLBACK_COMPRESSED_FRAME
   char* myData; // just some random data to demonstrate how data can be passed to the callback.
} COMPRESSED_WORKSPACE, *PCOMPRESSED_WORKSPACE;

// Use a global structure to pass data to/from the compressed callback
COMPRESSED_WORKSPACE gCompressionInfo = {{0,0},"The quick brown fox jumped over the lazy dog"};

// local prototypes
static PXL_RETURN_CODE getPixelFormat(HANDLE hCamera, U32* pixelFormat);
static PXL_RETURN_CODE getFrameSize(HANDLE hCamera, F32 bytesPerPixel, U32* frameSize);
static PXL_RETURN_CODE preview (HANDLE hCamera, bool on);

//
// This CALLBACK_FRAME callback function will be called when a decompressed image is available from the camera.
//
static U32 
FrameCallbackFunction(const HANDLE hCamera, void* const pFrameData, const U32 dataFormat, FRAME_DESC const * const pFrameDesc, void* const context)
{
   // This function should only ever receive uncompressed frames
   assert (pFrameDesc->CompressionInfo.fCompressionStrategy == FEATURE_COMPRESSION_STRATEGY_NONE);

   U32* frameData = (U32*)pFrameData;
   printf ("   Uncompressed -- FrameSize:%d FrameData(hex):%08X %08X %08X %08X\n", 
           (int)pFrameDesc->CompressionInfo.fCompressedSize, frameData[0], frameData[1], frameData[2], frameData[3]);

   return ApiSuccess;
}

//
// This CALLBACK_COMPRESSED_FRAME callback function will be called when a compressed image is available from the camera.
//
static U32
Pixelink10FrameCallbackFunction(const HANDLE hCamera, void* const pFrameData, const U32 dataFormat, FRAME_DESC const * const pFrameDesc, void* const context)
{
   // This function should only ever receive Pixelink10 compressed frames
   assert(pFrameDesc->CompressionInfo.fCompressionStrategy == FEATURE_COMPRESSION_STRATEGY_PIXELINK10);

   
   U32* frameData = (U32*)pFrameData;
   PCOMPRESSED_WORKSPACE pWorkspace = (PCOMPRESSED_WORKSPACE)context;
   printf("     Compressed -- FrameSize:%d FrameData(hex):%08X %08X %08X %08X myData:%s\n",
          (int)pFrameDesc->CompressionInfo.fCompressedSize, frameData[0], frameData[1], frameData[2], frameData[3], pWorkspace->myData);
   
   return ApiSuccess;
}


int main()
{
   //
   // Step 1
   //    Grab a camera
   PXL_RETURN_CODE rc;
   HANDLE           hCamera = NULL;

   rc = PxLInitialize(0, &hCamera);
   if (!API_SUCCESS(rc))
   {
      printf("  Could not find a camera.  RC:0x%X\n", rc);
      return GENERAL_ERROR;
   }

   //
   //  Step 2
   //       Make sure the camera is configured correctly:
   //         - Pixel format is either MONO8 or BAYER8
   //       And then determine the frame size
   U32 pixelFormat = PIXEL_FORMAT_MONO8;
   rc = getPixelFormat(hCamera, &pixelFormat);
   if (!API_SUCCESS(rc) ||
      (pixelFormat != PIXEL_FORMAT_MONO8 && !IS_BAYER8(pixelFormat)))
   {
      printf("  Unknown pixel format.  RC:0x%X, format:%d\n", rc, pixelFormat);
      PxLUninitialize(hCamera);
      return GENERAL_ERROR;
   }
   U32 frameSize = 0;
   rc = getFrameSize(hCamera, 1.0, &frameSize);
   if (!API_SUCCESS(rc))
   {
      printf("  Unknown frame size.  RC:0x%X\n", rc);
      PxLUninitialize(hCamera);
      return GENERAL_ERROR;
   }

   //
   // Step 3
   //    Enable compression
   U32 flags = NULL;
   U32 numParams = 2;
   F32 params[2];
   params[FEATURE_COMPRESSION_PARAM_PIXEL_FORMAT] = static_cast<F32>(pixelFormat);
   params[FEATURE_COMPRESSION_PARAM_STRATEGY] = FEATURE_COMPRESSION_STRATEGY_PIXELINK10;
   rc = PxLSetFeature(hCamera, FEATURE_COMPRESSION, FEATURE_FLAG_MANUAL, numParams, params);
   if (!API_SUCCESS(rc))
   {
      printf("  Cannot enable compression.  Are you sure this camera supports it?  RC:0x%X\n", rc);
      PxLUninitialize(hCamera);
      return GENERAL_ERROR;
   }

   //
   // Step 4
   //    Enable both the CALLBACK_FRAME and CALLBACK_COMPRESSED_FRAME type callbacks.  This may look a 
   //    little unusual, but this tells the API:
   //     - If the stream is not compressed, then return the uncompressed frames via FrameCallbackFunction.  However,
   //     - If the stream is compressed, then return the compressed frames via Pixelink10FrameCallbackFunction
   rc = PxLSetCallback(hCamera, CALLBACK_FRAME, NULL, FrameCallbackFunction);
   if (!API_SUCCESS(rc)) {
      printf("  Error: Could not set the frame callback\n");
      PxLUninitialize(hCamera);
      return GENERAL_ERROR;
   }
   // Be sure to set the compression strategy, so that the Pixelink API knows what type of compression this callback is for.
   gCompressionInfo.compressionInfo.compressionStrategy = FEATURE_COMPRESSION_STRATEGY_PIXELINK10;
   rc = PxLSetCallback(hCamera, CALLBACK_COMPRESSED_FRAME, &gCompressionInfo, Pixelink10FrameCallbackFunction);
   if (!API_SUCCESS(rc)) {
      printf("  Error: Could not set the compressed frame callback\n");
      PxLUninitialize(hCamera);
      return GENERAL_ERROR;
   }

   //
   // Step 5
   //    Enable the stream, with preview, for 3 seconds.
   //        Under these circumstances, each frame received from the camera will be decompressed (required for the preview) -- but
   //        the preview will show the decompressed variant, while the callback will receive the compressed variant.
   printf("  Enabling the stream with preview for 3 seconds -- you should see compressed callbacks + uncompressed preview ...\n");
   rc = PxLSetStreamState(hCamera, START_STREAM);
   if (!API_SUCCESS(rc))
   {
      printf("  Cannot start the stream.  RC:0x%X\n", rc);
      PxLUninitialize(hCamera);
      return GENERAL_ERROR;
   }
   rc = preview(hCamera, ON);
   if (!API_SUCCESS(rc))
   {
      printf("  Cannot start the preview.  RC:0x%X\n", rc);
      PxLSetStreamState(hCamera, STOP_STREAM);
      PxLUninitialize(hCamera);
      return GENERAL_ERROR;
   }
   sleep(3); // 3 seconds
   preview(hCamera, OFF);
   PxLSetStreamState(hCamera, STOP_STREAM);

   //
   // Step 6
   //    Enable the stream, without preview, for 3 seconds.
   //        Under these circumstances, each frame received from the camera will not be decompressed; the compressed
   //        frame will simply be returned via the callback.
   printf("  Enabling the stream without preview for 3 seconds -- you should see compressed callbacks ...\n");
   rc = PxLSetStreamState(hCamera, START_STREAM);
   if (!API_SUCCESS(rc))
   {
      printf("  Cannot start the stream.  RC:0x%X\n", rc);
      PxLUninitialize(hCamera);
      return GENERAL_ERROR;
   }
   sleep(3); // 3 seconds
   PxLSetStreamState(hCamera, STOP_STREAM);

   //
   // Step 7
   //    Cancel the callback for the compressed frames
   rc = PxLSetCallback(hCamera, CALLBACK_COMPRESSED_FRAME, NULL, NULL);
   if (!API_SUCCESS(rc)) {
      printf("  Error: Could not cancel the compressed frame callback\n");
      PxLUninitialize(hCamera);
      return GENERAL_ERROR;
   }

   //
   // Step 8
   //    Enable the stream for 3 seconds.
   printf("  Enabling the stream for 3 seconds -- you should see uncompressed callbacks...\n");
   rc = PxLSetStreamState(hCamera, START_STREAM);
   if (!API_SUCCESS(rc))
   {
      printf("  Cannot start the stream.  RC:0x%X\n", rc);
      PxLUninitialize(hCamera);
      return GENERAL_ERROR;
   }
   sleep(3); // 3 seconds
   PxLSetStreamState(hCamera, STOP_STREAM);

   // Step 9
   //    Cleanup
   PxLUninitialize(hCamera);

   return A_OK;
}

// Returns the pixel format currently being used by the specified camera.
static PXL_RETURN_CODE getPixelFormat(HANDLE hCamera, U32* pixelFormat)
{
   PXL_RETURN_CODE rc;

   U32 flags = NULL;
   U32 numParams = 1;
   F32 param;

   rc = PxLGetFeature(hCamera, FEATURE_PIXEL_FORMAT, &flags, &numParams, &param);
   if (API_SUCCESS(rc)) *pixelFormat = static_cast<U32>(param);

   return rc;
}

// Returns the frame size of the specified camera using the current camera settings.
static PXL_RETURN_CODE getFrameSize(HANDLE hCamera, F32 bytesPerPixel, U32* frameSize)
{
   PXL_RETURN_CODE rc;

   U32 flags = NULL;
   U32 numParams;
   F32 params[4];

   // 
   // Determine the ROI size
   numParams = 4;
   rc = PxLGetFeature(hCamera, FEATURE_ROI, &flags, &numParams, params);
   if (!API_SUCCESS(rc)) return rc;
   U32 width = static_cast<U32>(params[FEATURE_ROI_PARAM_WIDTH]);
   U32 height = static_cast<U32>(params[FEATURE_ROI_PARAM_HEIGHT]);

   //
   // Determine if there is any pixel addressing applied
   numParams = 4;
   rc = PxLGetFeature(hCamera, FEATURE_PIXEL_ADDRESSING, &flags, &numParams, params);
   if (!API_SUCCESS(rc)) return rc;
   U32 paX = static_cast<U32>(params[FEATURE_PIXEL_ADDRESSING_PARAM_X_VALUE]);
   U32 paY = static_cast<U32>(params[FEATURE_PIXEL_ADDRESSING_PARAM_Y_VALUE]);
   width = width / paX;
   height = height / paY;


   //
   // Determine if HDR Interleaved is applied
   U32 hdrInterleaveMultiplier = 1;
   numParams = 4;
   PXL_RETURN_CODE rc2 = PxLGetFeature(hCamera, FEATURE_GAIN_HDR, &flags, &numParams, params);
   if (API_SUCCESS(rc2))
   {
      if (*params == FEATURE_GAIN_HDR_MODE_INTERLEAVED) hdrInterleaveMultiplier = 2;
   }
   width = width * hdrInterleaveMultiplier;

   *frameSize = static_cast<U32>(static_cast<F32>(width * height) * bytesPerPixel);
   return rc;
}

static PXL_RETURN_CODE preview(const HANDLE hCamera, const bool on)
{
   PXL_RETURN_CODE rc;

   if (on)
   {
      rc = PxLSetPreviewState (hCamera, START_PREVIEW, NULL);
      if (API_SUCCESS(rc))
      {
         rc = PxLSetPreviewSettings (hCamera, "Preview", 0, 128, 128, 800, 600);
      }
   } else {
      rc = PxLSetPreviewState(hCamera, STOP_PREVIEW, NULL);
   }

   return rc;
}


