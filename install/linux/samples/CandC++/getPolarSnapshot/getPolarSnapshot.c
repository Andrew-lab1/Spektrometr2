//
// Sample code to capture and save 4 encoded BMP images from a Pixelink polar camera; 
// where each image represents each of the 4 polar channels.
//

#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>
#include "getPolarSnapshot.h"

//
// Check if polar weightings are supported by a camera.
// If polar weightings are supported, this is a polar camera.
//
// Returns TRUE if it is the polar camera
//
bool
IsPolarCamera(HANDLE hCamera) 
{
    PXL_RETURN_CODE rc = ApiUnknownError;
    bool isSupported = FALSE;
    U32 pBufferSize = 0;

    assert(0 != hCamera);

    // Figure out how much memory we have to allocate for feature polar weightings
    rc = PxLGetCameraFeatures(hCamera, FEATURE_POLAR_WEIGHTINGS, NULL, &pBufferSize);
    if (API_SUCCESS(rc))
    {
        CAMERA_FEATURES* pFeatureInfo = (CAMERA_FEATURES*)malloc(pBufferSize);
        if (NULL != pFeatureInfo) 
        {
            // Now read the information into the buffer
            rc = PxLGetCameraFeatures(hCamera, FEATURE_POLAR_WEIGHTINGS, pFeatureInfo, &pBufferSize);
            if (API_SUCCESS(rc))
            {
                // Do a few sanity checks
                assert(pFeatureInfo->uNumberOfFeatures == 1);
                assert(pFeatureInfo->pFeatures->uFeatureId == FEATURE_POLAR_WEIGHTINGS);

                // Is the feature polar weightings supported?
                isSupported = IS_FEATURE_SUPPORTED (pFeatureInfo->pFeatures->uFlags);
            
                free(pFeatureInfo);
                return isSupported;
            }

            free(pFeatureInfo);
        }
    }

    return isSupported;
}

//
// Set camera pixel format to Polar4_12
//
// Returns SUCCESS or FAILURE
//

int
SetPolarPixelFormat(HANDLE hCamera) 
{
    PXL_RETURN_CODE rc = ApiUnknownError;
    U32 flags = FEATURE_FLAG_MANUAL;
    U32 numParams = 1;
    float parms[1];

    assert(0 != hCamera);

    // Set pixel format to Polar4_12
    parms[0] = (F32)PIXEL_FORMAT_POLAR4_12;

    rc = PxLSetFeature(hCamera, FEATURE_PIXEL_FORMAT, flags, numParams, &parms[0]);

    return API_SUCCESS(rc) ? SUCCESS : FAILURE;
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
// Get a snapshot from the camera, and save to a file.
//
// Returns SUCCESS or FAILURE
//
int 
GetPolarSnapshot(HANDLE hCamera, U32 imageFormat, const char* pFileName, U32 polarChannel)
{
    U32 rawImageSize = 0;
    char* pRawImage;
    FRAME_DESC frameDesc;
    U32 encodedImageSize = 0;
    char* pEncodedImage;
    U32 polWeight0, polWeight45, polWeight90, polWeight135;
    int retVal = FAILURE;

    assert(0 != hCamera);
    assert(0 <= imageFormat);
    assert(NULL != pFileName);
    assert(0 <= polarChannel);

    // Set polar channels for a snapshot, where one polar channel has a weighting of 100%, and the others are 0.
    // If more than 4 snapshots taken, polar weighting for each polar channel is set to 100%.
    switch (polarChannel)
    {
        case FEATURE_POLAR_WEIGHTINGS_0_DEG:
            polWeight0 = 100;
            polWeight45 = 0;
            polWeight90 = 0;
            polWeight135 = 0;
            break;

        case FEATURE_POLAR_WEIGHTINGS_45_DEG:
            polWeight0 = 0;
            polWeight45 = 100;
            polWeight90 = 0;
            polWeight135 = 0;
            break;

        case FEATURE_POLAR_WEIGHTINGS_90_DEG:
            polWeight0 = 0;
            polWeight45 = 0;
            polWeight90 = 100;
            polWeight135 = 0;
            break;

        case FEATURE_POLAR_WEIGHTINGS_135_DEG:
            polWeight0 = 0;
            polWeight45 = 0;
            polWeight90 = 0;
            polWeight135 = 100;
            break;

        default:
            polWeight0 = 100;
            polWeight45 = 100;
            polWeight90 = 100;
            polWeight135 = 100;
            break;
    }

    // Set polar weightings for a snapshot
    if (SetPolarWeightings(hCamera, polWeight0, polWeight45, polWeight90, polWeight135) == FAILURE)
    {
        return retVal;
    }

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
// Set polar weighting for each polar channel
//
// Returns SUCCESS or FAILURE
//
int
SetPolarWeightings(HANDLE hCamera, U32 polWeight0, U32 polWeight45, U32 polWeight90, U32 polWeight135) 
{
    PXL_RETURN_CODE rc = ApiUnknownError;
    U32 flags = FEATURE_FLAG_MANUAL;
    U32 numParams = 4;
    float parms[4];

    assert(0 != hCamera);
    assert(0 <= polWeight0);
    assert(0 <= polWeight45);
    assert(0 <= polWeight90);
    assert(0 <= polWeight135);

    // Set polar weighting for each polar channel
    parms[FEATURE_POLAR_WEIGHTINGS_0_DEG] = (F32)polWeight0;
    parms[FEATURE_POLAR_WEIGHTINGS_45_DEG] = (F32)polWeight45;
    parms[FEATURE_POLAR_WEIGHTINGS_90_DEG] = (F32)polWeight90;
    parms[FEATURE_POLAR_WEIGHTINGS_135_DEG] = (F32)polWeight135;

    rc = PxLSetFeature(hCamera, FEATURE_POLAR_WEIGHTINGS, flags, numParams, &parms[0]);

    return API_SUCCESS(rc) ? SUCCESS : FAILURE;
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
    PXL_RETURN_CODE rc = ApiUnknownError;
    U32 flags = FEATURE_FLAG_MANUAL;
    U32 numParams = 0;
    float parms[4];        // reused for each feature query
    U32 roiWidth = 0;
    U32 roiHeight = 0;
    U32 numPixels = 0;
    U32 pixelFormat = 0;
    U32 pixelSize = 0;

    assert(0 != hCamera);

    // Get region of interest (ROI)
    numParams = 4; // left, top, width, height
    rc = PxLGetFeature(hCamera, FEATURE_ROI, &flags, &numParams, &parms[0]);
    if (!API_SUCCESS(rc))
    {
        return 0;
    }
    
    roiWidth = (U32)parms[FEATURE_ROI_PARAM_WIDTH];
    roiHeight = (U32)parms[FEATURE_ROI_PARAM_HEIGHT];

    // We can calulate the number of pixels now.
    // Value of pixel addressing feature is not included into calculation since a polar camera does not support it.
    numPixels = roiWidth * roiHeight;

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

    switch (pixelFormat) 
    {
    case PIXEL_FORMAT_STOKES4_12:
    case PIXEL_FORMAT_POLAR4_12:
    case PIXEL_FORMAT_HSV4_12:
        retVal = 6;
        break;

    default:
        assert(0);
        break;
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
    size_t numBytesWritten;
    FILE* pFile;

    assert(NULL != pFileName);
    assert(NULL != pImage);
    assert(0 < imageSize);

    // Open our file for binary write
    pFile = fopen(pFileName, "wb");
    if (NULL == pFile) {
        return FAILURE;
    }

    numBytesWritten = fwrite((void*)pImage, sizeof(char), imageSize, pFile);

    fclose(pFile);
    
    return ((U32)numBytesWritten == imageSize) ? SUCCESS : FAILURE;
}
