//
// getCompressedImage.cpp 
//
// Sample code to enable compression on the camera, grab a couple of images, and
// then report on the compression ratio achieved for the images.  This sample
// uses Pixelink10 compression scheme, and works with either mono or color cameras.
//

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <vector>
#include <unistd.h>
#include <PixeLINKApi.h>

using namespace std;

#define A_OK          0  // non-zero error codes
#define GENERAL_ERROR 1

#define IS_BAYER8(pixelFormat) \
   ((pixelFormat == PIXEL_FORMAT_BAYER8_RGGB) || \
    (pixelFormat == PIXEL_FORMAT_BAYER8_GBRG) || \
    (pixelFormat == PIXEL_FORMAT_BAYER8_BGGR) || \
    (pixelFormat == PIXEL_FORMAT_BAYER8_GRBG))

// local prototypes
static PXL_RETURN_CODE getPixelFormat(HANDLE hCamera, U32* pixelFormat);
static PXL_RETURN_CODE getFrameSize (HANDLE hCamera, F32 bytesPerPixel, U32* frameSize);
static int saveImageToFile (const char* pFilename, const U8* pImage, U32 imageSize);

static const char* pFile1 = "PxLGetNextFrameImage.bmp";
static const char* pFile2Bmp = "PxLGetNextCompressedFrameImage.bmp";
static const char* pFile2Raw = "PxLGetNextCompressedFrame.bin";

int main()
{

   //
   // Step 1
   //    Grab a camera
   PXL_RETURN_CODE rc;
   HANDLE           hCamera = NULL;

   rc = PxLInitialize (0, &hCamera);
   if (!API_SUCCESS(rc))
   {
      printf ("  Could not find a camera.  RC:0x%X\n", rc);
      return GENERAL_ERROR;
   }

   //
   //  Step 2
   //       Make sure the camera is configured correctly:
   //         - Pixel format is either MONO8 or BAYER8
   //       And then determine the frame size
   U32 pixelFormat = PIXEL_FORMAT_MONO8;
   rc = getPixelFormat (hCamera, &pixelFormat);
   if (!API_SUCCESS(rc) ||
       (pixelFormat != PIXEL_FORMAT_MONO8 && ! IS_BAYER8(pixelFormat)))
   {
      printf("  Unknown pixel format.  RC:0x%X, format:%d\n", rc, pixelFormat);
      PxLUninitialize (hCamera);
      return GENERAL_ERROR;
   }
   U32 frameSize = 0;
   rc = getFrameSize (hCamera, 1.0, &frameSize);
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
   rc = PxLSetFeature (hCamera, FEATURE_COMPRESSION, FEATURE_FLAG_MANUAL, numParams, params);
   if (!API_SUCCESS(rc))
   {
      printf("  Cannot enable compression.  Are you sure this camera supports it?  RC:0x%X\n", rc);
      PxLUninitialize(hCamera);
      return GENERAL_ERROR;
   }

   //
   // Step 4
   //    Enable the stream
   rc = PxLSetStreamState(hCamera, START_STREAM);
   if (!API_SUCCESS(rc))
   {
      printf("  Cannot start the stream.  RC:0x%X\n", rc);
      PxLUninitialize(hCamera);
      return GENERAL_ERROR;
   }

   // Step 5
   //    Initialize a few frame buffers, and then capture 2 frames; one compressed and one uncompressed.
   //    Note that PxLDecompressFrame requires that both the source and destination buffers be
   //    on a 64 byte boundary.

   // Frame 1
   U8* frame1Uncompressed = static_cast<U8*>(aligned_alloc (64, frameSize));
   FRAME_DESC frame1Desc;
   frame1Desc.uSize = sizeof (FRAME_DESC);
   // Frame 2
   U8* frame2Compressed = static_cast<U8*>(aligned_alloc (64, frameSize));
   U8* frame2Uncompressed = static_cast<U8*>(aligned_alloc (64, frameSize));
   FRAME_DESC frame2Desc;
   frame2Desc.uSize = sizeof(FRAME_DESC);
   U32 compressionDescSize = PIXELINK10_COMPRESSION_DESC_SIZE;
   vector<U8> compressionDesc(compressionDescSize, 0);
   U32 imageSize = 0;

   if (frame1Uncompressed == NULL || frame2Compressed == NULL || frame2Uncompressed == NULL) goto Cleanup;


   rc = PxLGetNextFrame(hCamera, frameSize, &frame1Uncompressed[0], &frame1Desc);
   if (API_SUCCESS(rc))
   {
      rc = PxLGetNextCompressedFrame(hCamera, frameSize, 
                                    &frame2Compressed[0], &frame2Desc,
                                    (COMPRESSION_DESC*)&compressionDesc[0], &compressionDescSize);
   }
   if (!API_SUCCESS(rc))
   {
      printf("  Could not capture the frames.  RC:0x%X\n", rc);
      goto Cleanup;
   }
   saveImageToFile(pFile2Raw, &frame2Compressed[0], frame2Desc.CompressionInfo.fCompressedSize);

   // 
   // Step 6
   //    Decompress frame 2
   rc = PxLDecompressFrame (&frame2Compressed[0], &frame2Desc, (COMPRESSION_DESC*)&compressionDesc[0], 
                            &frame2Uncompressed[0], &frameSize);
   if (!API_SUCCESS(rc))
   {
      printf("  Could not decompress frame 2.  RC:0x%X\n", rc);
      goto Cleanup;
   }

   //
   // Step 7
   //    Create a couple of bitmap images from the 2 captured frames.
   rc = PxLFormatImage (&frame1Uncompressed[0], &frame1Desc, IMAGE_FORMAT_BMP, NULL, &imageSize);
   if (API_SUCCESS(rc))
   {
      vector<U8> image(imageSize, 0);
      rc = PxLFormatImage(&frame1Uncompressed[0], &frame1Desc, IMAGE_FORMAT_BMP, &image[0], &imageSize);
      if (API_SUCCESS(rc))
      {
         saveImageToFile (pFile1, &image[0], imageSize);
      }
   }
   if (!API_SUCCESS(rc))
   {
      printf("  Could not save frame 1 as a BMP image.  RC:0x%X\n", rc);
      goto Cleanup;
   }

   rc = PxLFormatImage(&frame2Uncompressed[0], &frame2Desc, IMAGE_FORMAT_BMP, NULL, &imageSize);
   if (API_SUCCESS(rc))
   {
      vector<U8> image(imageSize, 0);
      rc = PxLFormatImage(&frame2Uncompressed[0], &frame2Desc, IMAGE_FORMAT_BMP, &image[0], &imageSize);
      if (API_SUCCESS(rc))
      {
         saveImageToFile(pFile2Bmp, &image[0], imageSize);
      }
   }
   if (!API_SUCCESS(rc))
   {
      printf("  Could not save frame 2 as a BMP image.  RC:0x%X\n", rc);
      goto Cleanup;
   }

   //
   // Step 8
   //    Report on the result
   printf("  Created %s; compressed %4.2f:1\n", pFile1, 
             (F32)frameSize / frame1Desc.CompressionInfo.fCompressedSize);
   printf("  Created %s; compressed %4.2f:1\n", pFile2Bmp,
             (F32)frameSize / frame2Desc.CompressionInfo.fCompressedSize);

Cleanup:
   PxLSetStreamState(hCamera, STOP_STREAM);
   PxLUninitialize(hCamera);
   if (frame1Uncompressed!= NULL)  free (frame1Uncompressed);
   if (frame2Compressed != NULL)   free (frame2Compressed);
   if (frame2Uncompressed != NULL) free (frame2Uncompressed);

   return API_SUCCESS(rc) ? A_OK : GENERAL_ERROR;
}

// Returns the pixel format currently being used by the specified camera.
static PXL_RETURN_CODE getPixelFormat(HANDLE hCamera, U32* pixelFormat)
{
   PXL_RETURN_CODE rc;

   U32 flags = NULL;
   U32 numParams = 1;
   F32 param;

   rc = PxLGetFeature(hCamera, FEATURE_PIXEL_FORMAT, &flags, &numParams, &param);
   if (API_SUCCESS (rc)) *pixelFormat = static_cast<U32>(param);

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
   rc = PxLGetFeature (hCamera, FEATURE_ROI, &flags, &numParams, params);
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

static int saveImageToFile(const char* pFilename, const U8* pImage, U32 imageSize)
{
   size_t numBytesWritten;
   FILE* pFile;

   assert(NULL != pFilename);
   assert(NULL != pImage);
   assert(imageSize > 0);

   // Open our file for binary write
   pFile = fopen(pFilename, "wb");
   if (NULL == pFile) {
      return GENERAL_ERROR;
   }

   numBytesWritten = fwrite((void*)pImage, sizeof(char), imageSize, pFile);

   fclose(pFile);

   return ((U32)numBytesWritten == imageSize) ? A_OK : GENERAL_ERROR;
}

