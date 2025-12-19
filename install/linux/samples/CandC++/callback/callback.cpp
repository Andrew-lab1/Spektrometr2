//
// callback.cpp 
//
// Demonstrates how to use callbacks with CALLBACK_FORMAT_IMAGE and CALLBACK_PREVIEW
// The callback functions here do not modify the image data. 
//
//
#include "PixeLINKApi.h"
#include "LinuxUtil.h"

#include <iostream>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/syscall.h>
#include <vector>

// Local function prototypes
static const char* GetPixelFormatAsString(U32 dataFormat);
static int GetBytesPerPixel(U32 dataFormat);
static void DoCallbackOnFormatImage(HANDLE hCamera);
static void	DoCallbackOnPreview(HANDLE hCamera);


int 
main(int argc, char* argv[])
{

	HANDLE hCamera;
	PXL_RETURN_CODE rc = PxLInitialize(0, &hCamera);
	if (!(API_SUCCESS(rc))) {
		printf("ERROR: 0x%8.8X\n", rc);
		return EXIT_FAILURE;
	}

	printf("Main thread id = %ld\n\n", syscall(SYS_gettid));

	DoCallbackOnFormatImage(hCamera);
	DoCallbackOnPreview(hCamera);

	PxLUninitialize(hCamera);
	return EXIT_SUCCESS;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//
// Callback function called by the API when an image is being converted via a call to PxLFormatImage.
// Note that the hCamera passed into this function will be 0 because it's API-global. i.e. there is only one
// callback function for PxLFormatImage. If you're controlling two cameras at once, any call to PxLFormatImage will
// end up invoking this function. 
//
// Note too that this function is called by the API using the same thread that called PxLFormatImage.
//
// N.B. The callback calling convention is __stdcall
//
//
static U32
CallbackFormatImage(
	HANDLE		hCamera,
	LPVOID		pData,
	U32			dataFormat,
	FRAME_DESC const *	pFrameDesc,
	LPVOID		userData)
{
	printf("CallbackFormatImage: hCamera=0x%p, pData=0x%p\n", hCamera, pData);
	printf("    dataFormat=%d (%s), pFrameDesc=0x%p\n", dataFormat, GetPixelFormatAsString(dataFormat), pFrameDesc);
	printf("    userData=0x%x, threadId=%ld\n", *(U32*)userData, syscall(SYS_gettid));
	printf("    imageData=0x%x %x %x %x %x %x %x %x\n\n", ((U8*)pData)[0], ((U8*)pData)[1], ((U8*)pData)[2], ((U8*)pData)[3], 
                                                              ((U8*)pData)[4], ((U8*)pData)[5], ((U8*)pData)[6], ((U8*)pData)[7]);
	return ApiSuccess;
}

static void 
DoCallbackOnFormatImage(HANDLE hCamera)
{
	// Set the callback function
	printf("============================================\n");
	printf("DoCallbackOnFormatImage\n");
	U32 userData = 0xCAFEBABE;
	printf("Registering FORMAT_IMAGE callback with userData 0x%8.8X\n", userData);

	PXL_RETURN_CODE rc = PxLSetCallback(
		0,			// for IMAGE_FORMAT_IMAGE, have to pass in 0
		CALLBACK_FORMAT_IMAGE,
		(LPVOID)&userData,
		CallbackFormatImage);
	if (!API_SUCCESS(rc)) {
		printf("ERROR setting callback function: 0x%8.8X\n", rc);
		return;
	}

	std::vector<U8> rawDataBuffer(3000*3000*2); // buffer big enough to hold any image
	FRAME_DESC frameDesc;

	rc = PxLSetStreamState(hCamera, START_STREAM);

	printf("Capturing a  frame...\n");
	while(1) {
		frameDesc.uSize = sizeof(FRAME_DESC);
		rc= PxLGetNextFrame(hCamera, (U32)rawDataBuffer.size(), &rawDataBuffer[0], &frameDesc);
		if (API_SUCCESS(rc)) {
			break;
		}
	}
	printf("frameDesc.PixelFormat=%d (%s)\n\n",(U32)frameDesc.PixelFormat.fValue, GetPixelFormatAsString((U32)frameDesc.PixelFormat.fValue));

	rc = PxLSetStreamState(hCamera, STOP_STREAM);

	printf("Calling PxLFormatImage with pBuffer=0x%p, &frameDesc=0x%p\n\n", &rawDataBuffer, &frameDesc);
	
	// And now format the image 
	// 1) How big is the resulting image going to be?
	// N.B. The callback function will not be invoked during this call
	U32 formattedImageSize = 0;
	rc = PxLFormatImage(&rawDataBuffer[0], &frameDesc, IMAGE_FORMAT_BMP, NULL, &formattedImageSize);

	std::vector<U8> formattedImage(formattedImageSize);

	// 2) Create the formatted image
	// The callback will be invoked during this call
	rc = PxLFormatImage(&rawDataBuffer[0], &frameDesc, IMAGE_FORMAT_BMP, &formattedImage[0], &formattedImageSize);

	// Disable the callback function
	rc = PxLSetCallback(0, CALLBACK_FORMAT_IMAGE, NULL, NULL);
	
}


/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//
// Callback function called by the API just before an image is displayed in the preview window. 
//
// N.B. This is called by the API on a thread created in the API. 
// N.B. The callback calling convention is __stdcall
//

static U32 
CallbackFormatPreview(
	HANDLE		hCamera,
	LPVOID		pData,
	U32			dataFormat,
	FRAME_DESC const *	pFrameDesc,
	LPVOID		userData)
{
	printf("CallbackFormatPreview: hCamera=0x%p, pData=0x%p\n", hCamera, pData);
	printf("    dataFormat=%d (%s), pFrameDesc=0x%p\n", dataFormat, GetPixelFormatAsString(dataFormat), pFrameDesc);
	printf("    userData=0x%x, threadId=%ld\n", *(U32*)userData, syscall(SYS_gettid));
	printf("    imageData=0x%x %x %x %x %x %x %x %x\n\n", ((U8*)pData)[0], ((U8*)pData)[1], ((U8*)pData)[2], ((U8*)pData)[3], 
                                                              ((U8*)pData)[4], ((U8*)pData)[5], ((U8*)pData)[6], ((U8*)pData)[7]);

        // Just to see the effect of the callback, increase the intensity of the middle 4% (20% of the width and 20% of the height)
        {
          int startRow = (int)((pFrameDesc->Roi.fHeight/5.0)*2.0);
          int endRow   = (int)((pFrameDesc->Roi.fHeight/5.0)*3.0);
          int startCol = (int)((pFrameDesc->Roi.fWidth/5.0)*2.0);
          int endCol   = (int)((pFrameDesc->Roi.fWidth/5.0)*3.0);

          // Find the first pixel that needs to be modified
          int bytesPerPixel = GetBytesPerPixel(dataFormat);

          U8* pPixel;

	  for (int i = startRow; i<endRow; i++)
          {
            pPixel = (U8*)pData + (bytesPerPixel * ((i * (int)pFrameDesc->Roi.fWidth) + startCol));
            
            for (int j = startCol; j<endCol; j++)
            {               
              for (int k=0; k<bytesPerPixel; k++)
              {
                if (pPixel[k] > pPixel[k] + pPixel[k]/5)
                {
                  // Already >= 80% saturated.  Make it fully saturated
                  pPixel[k] = 0xff;
                } else {
                  pPixel[k] = pPixel[k] + pPixel[k]/5; // Make it 20% brighter
                }
              }

              pPixel += bytesPerPixel;
            }

            pPixel = (U8*)pData + (bytesPerPixel * ((i * (int)pFrameDesc->Roi.fWidth) + startCol));
          }
        }
	return ApiSuccess;
}


static void 
DoCallbackOnPreview(HANDLE hCamera)
{
	// Set the callback function
	printf("============================================\n");
	printf("DoCallbackOnPreview\n");
	U32 userData = 0xDEADBEEF;
	printf("Registering PREVIEW callback with userData 0x%8.8X\n\n", userData);

	PXL_RETURN_CODE rc = PxLSetCallback(
		hCamera,			// for IMAGE_FORMAT_IMAGE, have to pass in 0
		CALLBACK_PREVIEW,
		(LPVOID)&userData,
		CallbackFormatPreview);
	if (!API_SUCCESS(rc)) {
		printf("ERROR setting callback function: 0x%8.8X\n", rc);
		return;
	}

	rc = PxLSetStreamState(hCamera, START_STREAM);

	// We will start getting our callback called after we start previewing
	HWND hWnd;
	rc = PxLSetPreviewState(hCamera, START_PREVIEW, &hWnd);

	sleep(5); // Sleep 5 seconds

	rc = PxLSetPreviewState(hCamera, STOP_PREVIEW, &hWnd);

	rc = PxLSetStreamState(hCamera, STOP_STREAM);

}


#define CASE(x) case x: return #x

static const char*
GetPixelFormatAsString(U32 dataFormat)
{
	switch(dataFormat) {
		CASE(PIXEL_FORMAT_MONO8);
		CASE(PIXEL_FORMAT_MONO16);
		CASE(PIXEL_FORMAT_YUV422);
		CASE(PIXEL_FORMAT_BAYER8_GRBG);
		CASE(PIXEL_FORMAT_BAYER16_GRBG);
		CASE(PIXEL_FORMAT_RGB24);
		CASE(PIXEL_FORMAT_RGB48);
		CASE(PIXEL_FORMAT_BAYER8_RGGB);
		CASE(PIXEL_FORMAT_BAYER8_GBRG);
		CASE(PIXEL_FORMAT_BAYER8_BGGR);
		CASE(PIXEL_FORMAT_BAYER16_RGGB);
		CASE(PIXEL_FORMAT_BAYER16_GBRG);
		CASE(PIXEL_FORMAT_BAYER16_BGGR);
                CASE(PIXEL_FORMAT_RGB24_NON_DIB);
		default: return "Unknown data format";
	}

}

static int
GetBytesPerPixel (U32 dataFormat)
{
	switch(dataFormat) {
        case PIXEL_FORMAT_MONO8:
        case PIXEL_FORMAT_BAYER8_GRBG:
        case PIXEL_FORMAT_BAYER8_RGGB:
        case PIXEL_FORMAT_BAYER8_GBRG:
        case PIXEL_FORMAT_BAYER8_BGGR:
	   return 1;
        case PIXEL_FORMAT_MONO16:
        case PIXEL_FORMAT_YUV422:
        case PIXEL_FORMAT_BAYER16_GRBG:
        case PIXEL_FORMAT_BAYER16_RGGB:
        case PIXEL_FORMAT_BAYER16_GBRG:
        case PIXEL_FORMAT_BAYER16_BGGR:
           return 2;
        case PIXEL_FORMAT_RGB24:
        case PIXEL_FORMAT_RGB24_NON_DIB:
           return 3;
        case PIXEL_FORMAT_RGB48:
           return 6;
        default:
           return 0;
        }
}
