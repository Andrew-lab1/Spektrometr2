//
// Sample code to capture an image from a PixeLINK camera and save 
// the encoded image to a file.
//

#include <PixeLINKApi.h>
#include "getsnapshot.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>


//
// Get a snapshot from the camera, and save to a file.
//
int 
GetSnapshot(HANDLE hCamera, U32 imageFormat, const char* pFilename)
{
	U32 rawImageSize;
	char* pRawImage;
	FRAME_DESC frameDesc;
	U32 encodedImageSize;
	char* pEncodedImage;
	int retVal = FAILURE;

	assert(0 != hCamera);
	assert(pFilename);

	// Determine the size of buffer we'll need to hold an 
	// image from the camera
	rawImageSize = DetermineRawImageSize(hCamera);
	if (0 == rawImageSize) {
		return FAILURE;
	}

	// Malloc the buffer to hold the raw image
	pRawImage = (char*)malloc(rawImageSize);
	if(NULL != pRawImage) {

		// Capture a raw image
		if (GetRawImage(hCamera, pRawImage, rawImageSize, &frameDesc) == SUCCESS) {

			//
			// Do any image processing here
			//

                        // Encode the raw image into something displayable
			if (EncodeRawImage(pRawImage, &frameDesc, imageFormat, &pEncodedImage, &encodedImageSize) == SUCCESS) {
				if (SaveImageToFile(pFilename, pEncodedImage, encodedImageSize) == SUCCESS) {
					retVal = SUCCESS;
				}
				free(pEncodedImage);
			}
		}
		free(pRawImage);
	}

	return retVal;
}

//
// Capture an image from the camera.
// 
// NOTE: PxLGetNextFrame is a blocking call. 
// i.e. PxLGetNextFrame won't return until an image is captured.
// So, if you're using hardware triggering, it won't return until the camera is triggered.
//
int
GetRawImage(HANDLE hCamera, char* pRawImage, U32 rawImageSize, FRAME_DESC* pFrameDesc)
{
	int retVal;

	assert(0 != hCamera);
	assert(NULL != pRawImage);
	assert(rawImageSize > 0);
	assert(NULL != pFrameDesc);


	// Put camera into streaming state so we can capture an image
	//if (!API_SUCCESS(PxLSetStreamState(hCamera, START_STREAM))) {
	//	return FAILURE;
	//}

	// Get an image
	retVal = GetNextFrame(hCamera, rawImageSize, (LPVOID*)pRawImage, pFrameDesc);

	// Done capturing, so no longer need the camera streaming images.
	//PxLSetStreamState(hCamera, STOP_STREAM);

	return (API_SUCCESS(retVal)) ? SUCCESS : FAILURE;
}

//
// NOTE: PxLGetNextFrame can return ApiCameraTimeoutError on occasion. 
// How you handle this depends on your situation and how you use your camera. 
// For this sample app, we'll just retry a few times.
//
PXL_RETURN_CODE 
GetNextFrame(HANDLE hCamera, U32 bufferSize, void* pFrame, FRAME_DESC* pFrameDesc)
{
	int numTries = 0;
	const int MAX_NUM_TRIES = 4;
	PXL_RETURN_CODE rc = ApiUnknownError;

	for(numTries = 0; numTries < MAX_NUM_TRIES; numTries++) {
		// Important that we set the frame desc size before each and every call to PxLGetNextFrame
		pFrameDesc->uSize = sizeof(FRAME_DESC);
		rc = PxLGetNextFrame(hCamera, bufferSize, pFrame, pFrameDesc);
		if (API_SUCCESS(rc)) {
			break;
		}
	}

	return rc;
}

//
// Query the camera for region of interest (ROI), decimation, and pixel format
// Using this information, we can calculate the size of a raw image
//
// Returns 0 on failure
//
U32 
DetermineRawImageSize(HANDLE hCamera)
{
	float parms[4];		// reused for each feature query
	U32 roiWidth;
	U32 roiHeight;
	U32 pixelAddressingValue;		// integral factor by which the image is reduced
	U32 pixelFormat;
	U32 numPixels;
	U32 pixelSize;
	U32 flags = FEATURE_FLAG_MANUAL;
	U32 numParams;

	assert(0 != hCamera);

	// Get region of interest (ROI)
	numParams = 4; // left, top, width, height
	PxLGetFeature(hCamera, FEATURE_ROI, &flags, &numParams, &parms[0]);
	roiWidth	= (U32)parms[FEATURE_ROI_PARAM_WIDTH];
	roiHeight	= (U32)parms[FEATURE_ROI_PARAM_HEIGHT];

	// Query pixel addressing 
	numParams = 2; // pixel addressing value, pixel addressing type (e.g. bin, average, ...)
	PxLGetFeature(hCamera, FEATURE_PIXEL_ADDRESSING, &flags, &numParams, &parms[0]);
	pixelAddressingValue = (U32)parms[FEATURE_PIXEL_ADDRESSING_PARAM_VALUE];

	// We can calulate the number of pixels now.
	numPixels = (roiWidth / pixelAddressingValue) * (roiHeight / pixelAddressingValue);

	// Knowing pixel format means we can determine how many bytes per pixel.
	numParams = 1;
	PxLGetFeature(hCamera, FEATURE_PIXEL_FORMAT, &flags, &numParams, &parms[0]);
	pixelFormat = (U32)parms[0];

	// And now the size of the frame
	pixelSize = GetPixelSize(pixelFormat);

	return numPixels * pixelSize;
}

//
// Given the pixel format, return the size of a individual pixel (in bytes)
//
// Returns 0 on failure.
//
U32 
GetPixelSize(U32 pixelFormat)
{
	U32 retVal = 0;

	switch(pixelFormat) {
	
		case PIXEL_FORMAT_MONO8:
		case PIXEL_FORMAT_BAYER8_GRBG:
		case PIXEL_FORMAT_BAYER8_RGGB:
		case PIXEL_FORMAT_BAYER8_GBRG:
		case PIXEL_FORMAT_BAYER8_BGGR:
			retVal = 1;
			break;

		case PIXEL_FORMAT_YUV422:
		case PIXEL_FORMAT_MONO16:
		case PIXEL_FORMAT_BAYER16_GRBG:
		case PIXEL_FORMAT_BAYER16_RGGB:
		case PIXEL_FORMAT_BAYER16_GBRG:
		case PIXEL_FORMAT_BAYER16_BGGR:
			retVal = 2;
			break;

		case PIXEL_FORMAT_RGB24:
			retVal = 3;
			break;

		case PIXEL_FORMAT_RGB48:
			retVal = 6;
			break;

		default:
			assert(0);
			break;
	}
	return retVal;
}

//
// Given a buffer with a raw image, create and return a 
// pointer to a new buffer with the encoded image. 
//
// NOTE: The caller becomes the owner of the buffer containing the 
//		 encoded image, and therefore must free the 
//		 buffer when done with it.
//
// Returns SUCCESS or FAILURE
//
int
EncodeRawImage(const char* pRawImage,
			   const FRAME_DESC* pFrameDesc, 
			   U32 encodedImageFormat, 
			   char** ppEncodedImage, 
			   U32* pEncodedImageSize)
{
	U32 encodedImageSize = 0;
	char* pEncodedImage;

	assert(NULL != pRawImage);
	assert(NULL != pFrameDesc);
	assert(NULL != ppEncodedImage);
	assert(NULL != pEncodedImageSize);

	// How big is the encoded image going to be?
	// Pass in NULL for the encoded image pointer, and the result is
	// returned in encodedImageSize
	if (API_SUCCESS(PxLFormatImage((LPVOID)pRawImage, (FRAME_DESC*)pFrameDesc, encodedImageFormat, NULL, &encodedImageSize))) {
		assert(encodedImageSize > 0);
		pEncodedImage = (char*)malloc(encodedImageSize);
		// Now that we have a buffer for the encoded image, ask for it to be converted.
		// NOTE: encodedImageSize is an IN param here because we're telling PxLFormatImage two things:
		//       1) pointer to the buffer
		//       2) the size of the buffer
		if (NULL != pEncodedImage) {
			if (API_SUCCESS(PxLFormatImage((LPVOID)pRawImage, (FRAME_DESC*)pFrameDesc, encodedImageFormat, pEncodedImage, &encodedImageSize))) {
				*ppEncodedImage = pEncodedImage; // handing over ownership of buffer to caller
				*pEncodedImageSize = encodedImageSize;
				return SUCCESS;
			}
			free(pEncodedImage);
        	}
	}

	return FAILURE;
}

//
// Save a buffer to a file
// This overwrites any existing file
//
// Returns SUCCESS or FAILURE
//
int
SaveImageToFile(const char* pFilename, const char* pImage, U32 imageSize)
{
	size_t numBytesWritten;
	FILE* pFile;

	assert(NULL != pFilename);
	assert(NULL != pImage);
	assert(imageSize > 0);

	// Open our file for binary write
	pFile = fopen(pFilename, "wb");
	if (NULL == pFile) {
		return FAILURE;
	}

	numBytesWritten = fwrite((void*)pImage, sizeof(char), imageSize, pFile);

	fclose(pFile);
	
	return ((U32)numBytesWritten == imageSize) ? SUCCESS : FAILURE;
}

