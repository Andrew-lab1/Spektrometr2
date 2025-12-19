//
// Sample code to capture images from an HDR Pixelink camera and save 
// each encoded image to a file.
//

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "getHDRSnapshot.h"

//
// Check if gain HDR is supported by a camera.
// If gain HDR is supported, this is an HDR camera.
//
// Returns TRUE if it is the HDR camera
//
bool
DoesCameraSupportHdr(HANDLE hCamera) {

    PXL_RETURN_CODE rc = ApiUnknownError;
    bool isSupported = FALSE;
    U32 pBufferSize = 0;

    assert(0 != hCamera);

    // Figure out how much memory we have to allocate for feature gain HDR
    rc = PxLGetCameraFeatures(hCamera, FEATURE_GAIN_HDR, NULL, &pBufferSize);
    if (API_SUCCESS(rc)) 
    {
        CAMERA_FEATURES* pFeatureInfo = (CAMERA_FEATURES*)malloc(pBufferSize);
        if(NULL != pFeatureInfo)
        {
            // Now read the information into the buffer
            rc = PxLGetCameraFeatures(hCamera, FEATURE_GAIN_HDR, pFeatureInfo, &pBufferSize);
            if (API_SUCCESS(rc)) 
            {
                // Do a few sanity checks
                assert(pFeatureInfo->uNumberOfFeatures == 1);
                assert(pFeatureInfo->pFeatures->uFeatureId == FEATURE_GAIN_HDR);

                // Is the feature gian HDR supported?
                isSupported = IS_FEATURE_SUPPORTED(pFeatureInfo->pFeatures->uFlags);

                free(pFeatureInfo);
                return isSupported;
            }

            free(pFeatureInfo);
        }
    }

    return isSupported;
}

//
// Create a file name for each snapshot
//
// Returns a string with a file name
//
char*
PrepareFileName(const char* pFileRoot, const char* pEnding)
{
    assert(NULL != pFileRoot);
    assert(NULL != pEnding);

    char* newString = malloc(strlen(pFileRoot) + strlen(pEnding) + 1);
    strcpy(newString, pFileRoot);
    strcat(newString, pEnding);
    return newString;
}

//
// Set gain HDR mode to "Camera HDR" or "Interleaved HDR"
// 
// Returns SUCCESS or FAILURE
//
int
SetHdrMode(HANDLE hCamera, U32 hdrMode)
{
    PXL_RETURN_CODE rc = ApiUnknownError;
    U32 flags;
    U32 numParams = 1;
    float parms[1];

    assert(0 != hCamera);

    // Set gain HDR mode to "Camera HDR" or "Interleaved HDR".
    // Disable gain HDR mode on default.
    switch (hdrMode)
    {
    // Set gain HDR mode to "Camera HDR"
    case FEATURE_GAIN_HDR_MODE_CAMERA:
        flags = FEATURE_FLAG_MANUAL;
        parms[0] = (F32)FEATURE_GAIN_HDR_MODE_CAMERA;
        break;

    // Set gain HDR mode to "Interleaved HDR"
    case FEATURE_GAIN_HDR_MODE_INTERLEAVED:
        flags = FEATURE_FLAG_MANUAL;
        parms[0] = (F32)FEATURE_GAIN_HDR_MODE_INTERLEAVED;
        break;
    
    // Disable gain HDR mode
    default:
        flags = FEATURE_FLAG_OFF;
        break;
    }

    rc = PxLSetFeature(hCamera, FEATURE_GAIN_HDR, flags, numParams, &parms[0]);
    
    return API_SUCCESS(rc) ? SUCCESS : FAILURE;
}

//
// Get a snapshot from the camera, and save to a file
//
// Returns SUCCESS or FAILURE
//
int 
GetHdrSnapshot(HANDLE hCamera, U32 imageFormat, const char* pFileName)
{
    U32 rawImageSize = 0;
    char* pRawImage;
    FRAME_DESC frameDesc;
    U32 encodedImageSize = 0;
    char* pEncodedImage;
    int retVal = FAILURE;

    assert(0 != hCamera);
    assert(0 <= imageFormat);
    assert(NULL != pFileName);

    // Determine the size of buffer we'll need to hold an 
    // image from the camera
    rawImageSize = DetermineRawImageSize(hCamera);
    if (0 == rawImageSize) 
    {
        return retVal;
    }

    // Malloc the buffer to hold the raw image
    pRawImage = (char*)malloc(rawImageSize);
    if(NULL != pRawImage) 
    {

    // Capture a raw image
        if (GetRawImage(hCamera, pRawImage, rawImageSize, &frameDesc) == SUCCESS) 
        {

            //
            // Do any image processing here
            //

            // Encode the raw image into something displayable
            if (EncodeRawImage(pRawImage, &frameDesc, imageFormat, &pEncodedImage, &encodedImageSize) == SUCCESS) 
            {
                if (SaveImageToFile(pFileName, pEncodedImage, encodedImageSize) == SUCCESS) 
                {
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
// Query the camera for region of interest (ROI), decimation, pixel format, and gain HDR mode.
// Using this information, we can calculate the size of a raw image.
//
// Returns 0 on failure
//
U32
DetermineRawImageSize(HANDLE hCamera)
{
    PXL_RETURN_CODE rc = ApiUnknownError;
    U32 flags = FEATURE_FLAG_MANUAL;
    U32 numParams = 0;
    float parms[4];     // reused for each feature query
    U32 roiWidth = 0;
    U32 roiHeight = 0;
    U32 pixelAddressingValueX, pixelAddressingValueY = 0;
    U32 numPixels = 0;
    U32 pixelFormat = 0;
    F32 pixelSize = 0.0f;
    U32 frameSize = 0;
    
    assert(0 != hCamera);

    // Get region of interest (ROI)
    numParams = FEATURE_ROI_NUM_PARAMS; // left, top, width, height
    rc = PxLGetFeature(hCamera, FEATURE_ROI, &flags, &numParams, &parms[0]);
    if (!API_SUCCESS(rc)) 
    {
        return 0;
    }
    roiWidth = (U32)parms[FEATURE_ROI_PARAM_WIDTH];
    roiHeight = (U32)parms[FEATURE_ROI_PARAM_HEIGHT];

    // Query pixel addressing 
    numParams = FEATURE_PIXEL_ADDRESSING_NUM_PARAMS; // pixel addressing value, pixel addressing type (e.g. bin, average, ...)
    rc = PxLGetFeature(hCamera, FEATURE_PIXEL_ADDRESSING, &flags, &numParams, &parms[0]);
    if (!API_SUCCESS(rc)) 
    {
        return 0;
    }
    // Width and height factor by which the image is reduced
    pixelAddressingValueX = (U32)parms[FEATURE_PIXEL_ADDRESSING_PARAM_X_VALUE];
    pixelAddressingValueY = (U32)parms[FEATURE_PIXEL_ADDRESSING_PARAM_Y_VALUE];

    // We can calulate the number of pixels now.
    numPixels = (roiWidth / pixelAddressingValueX) * (roiHeight / pixelAddressingValueY);

    // Knowing pixel format means we can determine how many bytes per pixel.
    numParams = 1;
    rc = PxLGetFeature(hCamera, FEATURE_PIXEL_FORMAT, &flags, &numParams, &parms[0]);
    if (!API_SUCCESS(rc))
    {
        return 0;
    }
    pixelFormat = (U32)parms[0];

    // And now the size of the frame
    pixelSize = GetPixelSize(pixelFormat);
    frameSize = (U32)(numPixels * pixelSize);
    
    // Check which gain HDR mode the camera is using
    numParams = 1;
    rc = PxLGetFeature(hCamera, FEATURE_GAIN_HDR, &flags, &numParams, &parms[0]);
    if (!API_SUCCESS(rc)) 
    {
        return 0;
    }

    // If the camera is using the interleaved HDR mode, double the size of the raw image
    if (parms[0] == FEATURE_GAIN_HDR_MODE_INTERLEAVED) 
    {
        return frameSize * 2;
    }

    return frameSize;
}

//
// Given the pixel format, return the size of an individual pixel (in bytes)
//
// Returns 0 on failure
//
F32
GetPixelSize(U32 pixelFormat)
{
    F32 retVal = 0.0f;

    switch (pixelFormat) 
    {
    case PIXEL_FORMAT_MONO8:
    case PIXEL_FORMAT_BAYER8_GRBG:
    case PIXEL_FORMAT_BAYER8_RGGB:
    case PIXEL_FORMAT_BAYER8_GBRG:
    case PIXEL_FORMAT_BAYER8_BGGR:
        retVal = 1.0f;
        break;

    case PIXEL_FORMAT_YUV422:
    case PIXEL_FORMAT_MONO16:
    case PIXEL_FORMAT_BAYER16_GRBG:
    case PIXEL_FORMAT_BAYER16_RGGB:
    case PIXEL_FORMAT_BAYER16_GBRG:
    case PIXEL_FORMAT_BAYER16_BGGR:
        retVal = 2.0f;
        break;

    case PIXEL_FORMAT_MONO12_PACKED_MSFIRST:
    case PIXEL_FORMAT_BAYER12_GRBG_PACKED_MSFIRST:
    case PIXEL_FORMAT_BAYER12_RGGB_PACKED_MSFIRST:
    case PIXEL_FORMAT_BAYER12_GBRG_PACKED_MSFIRST:
    case PIXEL_FORMAT_BAYER12_BGGR_PACKED_MSFIRST:
        retVal = 1.5f;
        break;

    case PIXEL_FORMAT_RGB24:
        retVal = 3.0f;
        break;

    case PIXEL_FORMAT_RGB48:
        retVal = 6.0f;
        break;

    default:
        assert(0);
        break;
    }
    return retVal;
}

//
// Capture an image from the camera
// 
// NOTE: PxLGetNextFrame is a blocking call. 
// i.e. PxLGetNextFrame won't return until an image is captured.
// So, if you're using hardware triggering, it won't return until the camera is triggered.
//
// Returns SUCCESS or FAILURE
//
int
GetRawImage(HANDLE hCamera, char* pRawImage, U32 rawImageSize, FRAME_DESC* pFrameDesc)
{
    PXL_RETURN_CODE rc = ApiUnknownError;

    assert(0 != hCamera);
    assert(NULL != pRawImage);
    assert(0 < rawImageSize);
    assert(NULL != pFrameDesc);


    // Put camera into streaming state so we can capture an image
    rc = PxLSetStreamState(hCamera, START_STREAM);
    if (!API_SUCCESS(rc)) 
    {
        return FAILURE;
    }

    // Get an image
    rc = GetNextFrame(hCamera, rawImageSize, (LPVOID*)pRawImage, pFrameDesc);
    if (!API_SUCCESS(rc))
    {
        return FAILURE;
    }

    // Done capturing, so no longer need the camera streaming images.
    rc = PxLSetStreamState(hCamera, STOP_STREAM);

    return API_SUCCESS(rc) ? SUCCESS : FAILURE;
}

//
// NOTE: PxLGetNextFrame can return ApiCameraTimeoutError on occasion. 
// How you handle this depends on your situation and how you use your camera. 
// For this sample app, we'll just retry a few times.
//
// Returns 0 on success
//
PXL_RETURN_CODE 
GetNextFrame(HANDLE hCamera, U32 bufferSize, void* pFrame, FRAME_DESC* pFrameDesc)
{
    PXL_RETURN_CODE rc = ApiUnknownError;
    int numTries = 0;
    const int MAX_NUM_TRIES = 4;

    assert(0 != hCamera);
    assert(0 < bufferSize);
    assert(NULL != pFrame);
    assert(NULL != pFrameDesc);

    for(numTries = 0; numTries < MAX_NUM_TRIES; numTries++) 
    {
        // Important that we set the frame desc size before each and every call to PxLGetNextFrame
        pFrameDesc->uSize = sizeof(FRAME_DESC);
        rc = PxLGetNextFrame(hCamera, bufferSize, pFrame, pFrameDesc);
        if (API_SUCCESS(rc)) 
        {
            break;
        }
    }

    return rc;
}

//
// Given a buffer with a raw image, create and return a 
// pointer to a new buffer with the encoded image. 
//
// NOTE: The caller becomes the owner of the buffer containing the 
//       encoded image, and therefore must free the 
//       buffer when done with it.
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
    PXL_RETURN_CODE rc = ApiUnknownError;
    U32 encodedImageSize = 0;
    char* pEncodedImage;

    assert(NULL != pRawImage);
    assert(NULL != pFrameDesc);
    assert(0 <= encodedImageFormat);
    assert(NULL != ppEncodedImage);
    assert(NULL != pEncodedImageSize);

    // How big is the encoded image going to be?
    // Pass in NULL for the encoded image pointer, and the result is
    // returned in encodedImageSize
    rc = PxLFormatImage((LPVOID)pRawImage, (FRAME_DESC*)pFrameDesc, encodedImageFormat, NULL, &encodedImageSize);
    if (API_SUCCESS(rc)) 
    {
        assert(0 < encodedImageSize);
        pEncodedImage = (char*)malloc(encodedImageSize);
        // Now that we have a buffer for the encoded image, ask for it to be converted.
        // NOTE: encodedImageSize is an IN param here because we're telling PxLFormatImage two things:
        //       1) pointer to the buffer
        //       2) the size of the buffer
        if (NULL != pEncodedImage) 
        {
            rc = PxLFormatImage((LPVOID)pRawImage, (FRAME_DESC*)pFrameDesc, encodedImageFormat, pEncodedImage, &encodedImageSize);
            if (API_SUCCESS(rc)) 
            {
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
SaveImageToFile(const char* pFileName, const char* pImage, U32 imageSize)
{
    size_t numBytesWritten = 0;
    FILE* pFile;

    assert(NULL != pFileName);
    assert(NULL != pImage);
    assert(0 < imageSize);

    // Open our file for binary write
    pFile = fopen(pFileName, "wb");
    if (NULL == pFile) 
    {
        return FAILURE;
    }

    numBytesWritten = fwrite((void*)pImage, sizeof(char), imageSize, pFile);

    fclose(pFile);
    
    return ((U32)numBytesWritten == imageSize) ? SUCCESS : FAILURE;
}
