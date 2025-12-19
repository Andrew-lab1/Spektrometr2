/***************************************************************************
 *
 *     File: camera.cpp
 *
 *     Description: Class definition for a very simple camera.
 */

#include "captureOEMLite.h"
#include "utility.h"

#include <unistd.h>
#include <vector>
#include <memory>

using namespace std;

// define a macro that will conveniently interrupt the stream is needed to make a feature adjustment
#define STOP_STREAM_IF_REQUIRED(FEATURE)                                                    \
    std::auto_ptr<PxLInterruptStream> _temp_ss(NULL);                                             \
    if (requiresStreamStop(FEATURE))                                                     \
        _temp_ss = std::auto_ptr<PxLInterruptStream>(new PxLInterruptStream(this, STOP_STREAM));  \

// Prototypes
extern "C" void StopButtonPressed
  (GtkWidget* widget, GdkEventExpose* event, gpointer userdata);

/***********************************************************************
 *  Global Functions
 */

static gboolean previewStop (gpointer pData)
{
	// If the user presses the little red X in the preview window, then the API
	// treats this just like the user calling PxLSetPreviewState STOP_PREVIEW.  We
	// want to make this look the same as the user having pressed the stop button.

	StopButtonPressed (NULL, NULL, NULL);

	return false; //  false == only run once
}

extern "C" U32 previewWindowEvent
  (HANDLE hCamera, U32 event, LPVOID pdata)
{
	if (PREVIEW_CLOSED == event)
	{
		gdk_threads_add_idle ((GSourceFunc)previewStop, NULL);
	}

	return 0;
}

/***********************************************************************
 *  Public members
 */


PxLCamera::PxLCamera (ULONG serialNum)
: m_serialNum(0)
, m_hCamera(NULL)
, m_streamState(STOP_STREAM)
, m_previewState(STOP_PREVIEW)
{
	PXL_RETURN_CODE rc = ApiSuccess;
	char  title[40];

	rc = PxLInitializeEx (serialNum, &m_hCamera, 0);
	//if (!API_SUCCESS(rc) && rc != ApiNoCameraError)
	if (!API_SUCCESS(rc))
	{
		throw PxLError(rc);
	}
	m_serialNum = serialNum;

	// Set the preview window to a fixed size.
	sprintf (title, "Preview - Camera %d", m_serialNum);
	PxLSetPreviewSettings (m_hCamera, title, 0, 128, 128, 1024, 768);

	// Create our poller object
	m_poller = new PxLFeaturePoller (1000);

}

PxLCamera::~PxLCamera()
{
	delete m_poller;

    PxLUninitialize (m_hCamera);
}

PXL_RETURN_CODE PxLCamera::play()
{
	PXL_RETURN_CODE rc = ApiSuccess;
	ULONG currentStreamState = m_streamState;

	// Start the camera stream, if necessary
	if (START_STREAM != currentStreamState)
	{
		rc = PxLSetStreamState (m_hCamera, START_STREAM);
		if (!API_SUCCESS(rc)) return rc;
	}

	// now, start the preview
	rc = PxLSetPreviewStateEx(m_hCamera, START_PREVIEW, &m_previewHandle, NULL, previewWindowEvent);
	if (!API_SUCCESS(rc))
	{
		PxLSetStreamState (m_hCamera, currentStreamState);
		m_streamState = currentStreamState;
		return rc;
	}
	m_streamState = START_STREAM;
	m_previewState = START_PREVIEW;

	return ApiSuccess;
}

PXL_RETURN_CODE PxLCamera::pause()
{
	PXL_RETURN_CODE rc = ApiSuccess;

	rc = PxLSetPreviewState (m_hCamera, PAUSE_PREVIEW, &m_previewHandle);
	if (!API_SUCCESS(rc)) return rc;

	// We we wanted, we can also pause the stream.  This will make the bus quieter, and
	// a little less load on the system.
	rc = PxLSetStreamState (m_hCamera, PAUSE_STREAM);
	if (!API_SUCCESS(rc)) return rc;
	m_streamState = PAUSE_STREAM;

	m_previewState = PAUSE_PREVIEW;

	return ApiSuccess;
}

PXL_RETURN_CODE PxLCamera::stop()
{
	PXL_RETURN_CODE rc = ApiSuccess;

	rc = PxLSetPreviewState(m_hCamera, STOP_PREVIEW, &m_previewHandle);
	if (!API_SUCCESS(rc)) return rc;
	m_previewState = STOP_PREVIEW;

	rc = PxLSetStreamState (m_hCamera, STOP_STREAM);
	m_streamState = STOP_STREAM;

	return rc;
}

PXL_RETURN_CODE PxLCamera::resizePreviewToRoi()
{
    return PxLResetPreviewWindow(m_hCamera);
}

bool PxLCamera::supported (ULONG feature)
{
    PXL_RETURN_CODE rc = ApiSuccess;
    ULONG  flags = 0;

    rc = getFlags (feature, &flags);
    if (!API_SUCCESS(rc)) return false;

    return (IS_FEATURE_SUPPORTED(flags));
}

bool PxLCamera::oneTimeSuppored (ULONG feature)
{
    PXL_RETURN_CODE rc = ApiSuccess;
    ULONG  flags = 0;

    rc = getFlags (feature, &flags);
    if (!API_SUCCESS(rc)) return false;

    return (0 != (flags & FEATURE_FLAG_ONETIME));
}

bool PxLCamera::continuousSupported (ULONG feature)
{
    PXL_RETURN_CODE rc = ApiSuccess;
    ULONG  flags = 0;

    rc = getFlags (feature, &flags);
    if (!API_SUCCESS(rc)) return false;

    return (0 != (flags & FEATURE_FLAG_AUTO));
}

PXL_RETURN_CODE PxLCamera::getRange (ULONG feature, float* min, float* max)
{
    PXL_RETURN_CODE rc = ApiSuccess;
    ULONG  featureSize = 0;

    rc = PxLGetCameraFeatures (m_hCamera, feature, NULL, &featureSize);
    if (!API_SUCCESS(rc)) return rc;
    vector<BYTE> featureStore(featureSize);
    PCAMERA_FEATURES pFeatureInfo= (PCAMERA_FEATURES)&featureStore[0];
    rc = PxLGetCameraFeatures (m_hCamera, feature, pFeatureInfo, &featureSize);
    if (!API_SUCCESS(rc)) return rc;

    if (1 != pFeatureInfo->uNumberOfFeatures ||
        NULL == pFeatureInfo->pFeatures ||
        NULL == pFeatureInfo->pFeatures->pParams) return ApiInvalidParameterError;

    *min = pFeatureInfo->pFeatures->pParams->fMinValue;
    *max = pFeatureInfo->pFeatures->pParams->fMaxValue;

    return ApiSuccess;
}

PXL_RETURN_CODE PxLCamera::getValue (ULONG feature, float* value)
{
    PXL_RETURN_CODE rc = ApiSuccess;
    float featureValue;
    ULONG flags;
    ULONG numParams = 1;

    rc = PxLGetFeature (m_hCamera, feature, &flags, &numParams, &featureValue);
    if (!API_SUCCESS(rc)) return rc;

    *value = featureValue;

    return ApiSuccess;
}

PXL_RETURN_CODE PxLCamera::setValue (ULONG feature, float value)
{
    PXL_RETURN_CODE rc = ApiSuccess;

    STOP_STREAM_IF_REQUIRED(feature);

    rc = PxLSetFeature (m_hCamera, feature, FEATURE_FLAG_MANUAL, 1, &value);

    return rc;
}

PXL_RETURN_CODE PxLCamera::performOneTimeAuto (ULONG feature)
{
    PXL_RETURN_CODE rc = ApiSuccess;
    float value[3];
    ULONG flags = FEATURE_FLAG_ONETIME;
    ULONG numParameters = FEATURE_WHITE_SHADING == feature ? 3 : 1;

    STOP_STREAM_IF_REQUIRED(feature);

    PxLWaitCursor waitCursor;  // Show a wait cursor while we are waiting for the operation to complete

    rc = PxLSetFeature (m_hCamera, feature, flags, numParameters, &value[0]);
    if (!API_SUCCESS(rc)) return rc;

    // OK, here is the tricky part.  We have started a one-time auto adjustment of the feature.  But
    // how do we know when it is done?  From a camera's perspective, we can call PxLGetFeature and
    // check to see of the FEATURE_FLAG_ONETIME is set in the flags.  If it is not, then it has completed
    // successfully.  But what if it hasn't?  Well, we could spin for a while waiting for it to
    // complete.  However, if we are going to do that, then we 'should' have some mechanism for the
    // user to quit the operation.  If we do decide to abort the auto operation, all we have to do is
    // call PxLSetFeature again, but this time, with a clear FEATURE_FLAG_ONETIME bit in flags.
    //
    // This app is going to take a very simple approach of assuming the one-time will complete successfully at
    // some point.  So, we will just stall for a little bit and return.  On the off chance the auto algorithm
    // did not converge below, then the oneTime operation will be cancelled the next time the user sets the
    // feature.
    for (int i=0; i<20; i++)
    {
        rc = PxLGetFeature (m_hCamera, feature, &flags, &numParameters, &value[0]);
        if (!API_SUCCESS(rc)) return rc;

        if (!(flags & FEATURE_FLAG_ONETIME)) break;  //Whoo-hoo, it's done.
        usleep (1000*500); // 500 ms.
    }

    return ApiSuccess;
}

PXL_RETURN_CODE PxLCamera::getContinuousAuto (ULONG feature, bool* enabled)
{
    PXL_RETURN_CODE rc = ApiSuccess;
    float featureValue;
    ULONG flags;
    ULONG numParams = 1;

    rc = PxLGetFeature (m_hCamera, feature, &flags, &numParams, &featureValue);
    if (!API_SUCCESS(rc)) return rc;

    *enabled = (0 != (flags & FEATURE_FLAG_AUTO));

    return ApiSuccess;
}


PXL_RETURN_CODE PxLCamera::setContinuousAuto (ULONG feature, bool enable)
{
    PXL_RETURN_CODE rc = ApiSuccess;
    float value;
    ULONG flags = 0;
    ULONG numParameters = 1;

    STOP_STREAM_IF_REQUIRED(feature);

    if (! enable)
    {
        // We are disabling continuous auto adjustment, and restoring manual adjustment.
        // When we set the feature (to turn off continuous), we have to set the feature to
        // 'something' -- so read the current value so that we can use it.
        rc = PxLGetFeature (m_hCamera, feature, &flags, &numParameters, &value);
        if (!API_SUCCESS(rc)) return rc;
    }

    flags = enable ? FEATURE_FLAG_AUTO : FEATURE_FLAG_MANUAL;

    rc = PxLSetFeature (m_hCamera, feature, flags, numParameters, &value);

    return rc;
}

PXL_RETURN_CODE PxLCamera::getPixelAddressRange (float* minMode, float* maxMode, float* minValue, float* maxValue)
{
    PXL_RETURN_CODE rc = ApiSuccess;
    ULONG  featureSize = 0;

    rc = PxLGetCameraFeatures (m_hCamera, FEATURE_PIXEL_ADDRESSING, NULL, &featureSize);
    if (!API_SUCCESS(rc)) return rc;
    vector<BYTE> featureStore(featureSize);
    PCAMERA_FEATURES pFeatureInfo= (PCAMERA_FEATURES)&featureStore[0];
    rc = PxLGetCameraFeatures (m_hCamera, FEATURE_PIXEL_ADDRESSING, pFeatureInfo, &featureSize);
    if (!API_SUCCESS(rc)) return rc;

    if (1 != pFeatureInfo->uNumberOfFeatures ||
        NULL == pFeatureInfo->pFeatures ||
        NULL == pFeatureInfo->pFeatures->pParams ||
        pFeatureInfo->pFeatures->uNumberOfParameters < 2) return ApiInvalidParameterError;

    *minMode = pFeatureInfo->pFeatures->pParams[FEATURE_PIXEL_ADDRESSING_PARAM_MODE].fMinValue;
    *maxMode = pFeatureInfo->pFeatures->pParams[FEATURE_PIXEL_ADDRESSING_PARAM_MODE].fMaxValue;
    *minValue = pFeatureInfo->pFeatures->pParams[FEATURE_PIXEL_ADDRESSING_PARAM_VALUE].fMinValue;
    *maxValue = pFeatureInfo->pFeatures->pParams[FEATURE_PIXEL_ADDRESSING_PARAM_VALUE].fMaxValue;

    return ApiSuccess;
}

PXL_RETURN_CODE PxLCamera::getPixelAddressValue (float* mode, float* value)
{
    PXL_RETURN_CODE rc = ApiSuccess;
    ULONG numParams = 4;
    float featureValue[numParams];
    ULONG flags;

    rc = PxLGetFeature (m_hCamera, FEATURE_PIXEL_ADDRESSING, &flags, &numParams, featureValue);
    if (!API_SUCCESS(rc)) return rc;

    *mode = featureValue[FEATURE_PIXEL_ADDRESSING_PARAM_MODE];
    *value = featureValue[FEATURE_PIXEL_ADDRESSING_PARAM_VALUE];

    return ApiSuccess;
}

PXL_RETURN_CODE PxLCamera::setPixelAddressValue (float mode, float value)
{
    PXL_RETURN_CODE rc = ApiSuccess;
    ULONG numParams = 2; // Only use 2 parameters as we are not using asymmetric pixel addressing
    float featureValue[numParams];

    STOP_STREAM_IF_REQUIRED(FEATURE_PIXEL_ADDRESSING);

    featureValue[FEATURE_PIXEL_ADDRESSING_PARAM_MODE] = mode;
    featureValue[FEATURE_PIXEL_ADDRESSING_PARAM_VALUE] = value;

    rc = PxLSetFeature (m_hCamera, FEATURE_PIXEL_ADDRESSING, FEATURE_FLAG_MANUAL, numParams, featureValue);

    return rc;
}

PXL_RETURN_CODE PxLCamera::getRoiRange (PXL_ROI* minRoi, PXL_ROI* maxRoi)
{
    PXL_RETURN_CODE rc = ApiSuccess;
    ULONG  featureSize = 0;

    rc = PxLGetCameraFeatures (m_hCamera, FEATURE_ROI, NULL, &featureSize);
    if (!API_SUCCESS(rc)) return rc;
    vector<BYTE> featureStore(featureSize);
    PCAMERA_FEATURES pFeatureInfo= (PCAMERA_FEATURES)&featureStore[0];
    rc = PxLGetCameraFeatures (m_hCamera, FEATURE_ROI, pFeatureInfo, &featureSize);
    if (!API_SUCCESS(rc)) return rc;

    if (1 != pFeatureInfo->uNumberOfFeatures ||
        NULL == pFeatureInfo->pFeatures ||
        NULL == pFeatureInfo->pFeatures->pParams ||
        4 < pFeatureInfo->pFeatures->uNumberOfParameters) return ApiInvalidParameterError;

    minRoi->m_width = (int)pFeatureInfo->pFeatures->pParams[FEATURE_ROI_PARAM_WIDTH].fMinValue;
    maxRoi->m_width = (int)pFeatureInfo->pFeatures->pParams[FEATURE_ROI_PARAM_WIDTH].fMaxValue;
    minRoi->m_height = (int)pFeatureInfo->pFeatures->pParams[FEATURE_ROI_PARAM_HEIGHT].fMinValue;
    maxRoi->m_height = (int)pFeatureInfo->pFeatures->pParams[FEATURE_ROI_PARAM_HEIGHT].fMaxValue;

    return ApiSuccess;
}

PXL_RETURN_CODE PxLCamera::getRoiValue (PXL_ROI* roi)
{
    PXL_RETURN_CODE rc = ApiSuccess;
    ULONG numParams = 4;
    float featureValue[numParams];
    ULONG flags;

    rc = PxLGetFeature (m_hCamera, FEATURE_ROI, &flags, &numParams, featureValue);
    if (!API_SUCCESS(rc)) return rc;

    roi->m_width = featureValue[FEATURE_ROI_PARAM_WIDTH];
    roi->m_height = featureValue[FEATURE_ROI_PARAM_HEIGHT];

    return ApiSuccess;
}

PXL_RETURN_CODE PxLCamera::setRoiValue (PXL_ROI &roi)
{
    PXL_RETURN_CODE rc = ApiSuccess;
    ULONG numParams = 4;
    float featureValue[numParams];

    STOP_STREAM_IF_REQUIRED(FEATURE_ROI);

    featureValue[FEATURE_ROI_PARAM_WIDTH] = (float)roi.m_width;
    featureValue[FEATURE_ROI_PARAM_HEIGHT] = (float)roi.m_height;
    // We will always center our ROI...
    PXL_ROI min,max;
    int roiOriginX, roiOriginY;

    rc = getRoiRange (&min, &max);
    if (!API_SUCCESS(rc)) return rc;

    roiOriginX = (((max.m_width - roi.m_width) / 2) / min.m_width) * min.m_width;
    roiOriginY = (((max.m_height - roi.m_height) / 2) / min.m_height) * min.m_height;
    featureValue[FEATURE_ROI_PARAM_LEFT] = (float)roiOriginX;
    featureValue[FEATURE_ROI_PARAM_TOP] = (float)roiOriginY;

    rc = PxLSetFeature (m_hCamera, FEATURE_ROI, FEATURE_FLAG_MANUAL, numParams, featureValue);

    return rc;
}

PXL_RETURN_CODE PxLCamera::getWhiteBalanceRange (float* min, float* max)
{
    PXL_RETURN_CODE rc = ApiSuccess;
    ULONG  featureSize = 0;

    rc = PxLGetCameraFeatures (m_hCamera, FEATURE_WHITE_SHADING, NULL, &featureSize);
    if (!API_SUCCESS(rc)) return rc;
    vector<BYTE> featureStore(featureSize);
    PCAMERA_FEATURES pFeatureInfo= (PCAMERA_FEATURES)&featureStore[0];
    rc = PxLGetCameraFeatures (m_hCamera, FEATURE_WHITE_SHADING, pFeatureInfo, &featureSize);
    if (!API_SUCCESS(rc)) return rc;

    if (1 != pFeatureInfo->uNumberOfFeatures ||
        NULL == pFeatureInfo->pFeatures ||
        NULL == pFeatureInfo->pFeatures->pParams) return ApiInvalidParameterError;

    // make sure there are 3 parameters, and the min and max of all 3 are the same value.
    if (pFeatureInfo->pFeatures->uNumberOfParameters != 3 ||
        pFeatureInfo->pFeatures->pParams[0].fMinValue != pFeatureInfo->pFeatures->pParams[1].fMinValue ||
        pFeatureInfo->pFeatures->pParams[0].fMinValue != pFeatureInfo->pFeatures->pParams[2].fMinValue ||
        pFeatureInfo->pFeatures->pParams[0].fMaxValue != pFeatureInfo->pFeatures->pParams[1].fMaxValue ||
        pFeatureInfo->pFeatures->pParams[0].fMaxValue != pFeatureInfo->pFeatures->pParams[2].fMaxValue)
       return ApiInvalidParameterError;

    *min = pFeatureInfo->pFeatures->pParams->fMinValue;
    *max = pFeatureInfo->pFeatures->pParams->fMaxValue;

    return ApiSuccess;
}

PXL_RETURN_CODE PxLCamera::getWhiteBalanceValues (float* red, float* green, float* blue)
{
    PXL_RETURN_CODE rc = ApiSuccess;
    float featureValues[3];
    ULONG flags;
    ULONG numParams = 3;

    rc = PxLGetFeature (m_hCamera, FEATURE_WHITE_SHADING, &flags, &numParams, featureValues);
    if (!API_SUCCESS(rc)) return rc;

    *red   = featureValues[0];
    *green = featureValues[1];
    *blue  = featureValues[2];

    return ApiSuccess;
}

PXL_RETURN_CODE PxLCamera::setWhiteBalanceValues (float red, float green, float blue)
{
    PXL_RETURN_CODE rc = ApiSuccess;

    STOP_STREAM_IF_REQUIRED(FEATURE_WHITE_SHADING);

    float featureValues[3];
    featureValues[0] = red;
    featureValues[1] = green;
    featureValues[2] = blue;

    rc = PxLSetFeature (m_hCamera, FEATURE_WHITE_SHADING, FEATURE_FLAG_MANUAL, 3, featureValues);

    return rc;
}

PXL_RETURN_CODE PxLCamera::getFlip (bool* horizontal, bool* vertical)
{
    PXL_RETURN_CODE rc = ApiSuccess;
    float featureValues[2];
    ULONG flags;
    ULONG numParams = 2;

    rc = PxLGetFeature (m_hCamera, FEATURE_FLIP, &flags, &numParams, featureValues);
    if (!API_SUCCESS(rc)) return rc;

    *horizontal = featureValues[0] != 0.0f;
    *vertical   = featureValues[0] != 0.0f;

    return ApiSuccess;
}

PXL_RETURN_CODE PxLCamera::setFlip (bool horizontal, bool vertical)
{
    PXL_RETURN_CODE rc = ApiSuccess;

    STOP_STREAM_IF_REQUIRED(FEATURE_WHITE_SHADING);

    float featureValues[2];
    featureValues[0] = horizontal ? 1.0 : 0.0;
    featureValues[1] = vertical ? 1.0 : 0.0;

    rc = PxLSetFeature (m_hCamera, FEATURE_FLIP, FEATURE_FLAG_MANUAL, 2, featureValues);

    return rc;
}

PXL_RETURN_CODE PxLCamera::captureImage (const char* fileName, ULONG imageType)
{
    PXL_RETURN_CODE rc = ApiSuccess;

    U32 rawImageSize;

    assert(0 != m_hCamera);
    assert(fileName);
    assert (streaming());

    //
    // Step 1.
    //     Determine the size of buffer we'll need to hold an
    //     image from the camera, and allocate a buffer.
    rawImageSize = imageSize();
    if (0 == rawImageSize) return ApiBadFrameSizeError;

    vector<char>pRawImage(rawImageSize);
    FRAME_DESC frameDesc;

    //
    // Step 2.
    //      Grab an image
    rc = getNextFrame (rawImageSize, &pRawImage[0], &frameDesc);
    if (!API_SUCCESS(rc)) return rc;

    //
    // Step 3.
    //      Format the image (if necessary).
    U32   encodedImageSize = 0;
    char* pEncodedImage;
    vector<char>pEncodedImageData;
    if (IMAGE_FORMAT_RAW != imageType)
    {
        // first, figure out how much storage I need, then allocate a buffer.
        rc = PxLFormatImage(&pRawImage[0], &frameDesc, imageType, NULL, &encodedImageSize);
        if (!API_SUCCESS(rc)) return rc;
        if (0 == encodedImageSize) return ApiBadFrameSizeError;
        pEncodedImageData.resize(encodedImageSize);
        pEncodedImage = &pEncodedImageData[0];
        rc = PxLFormatImage(&pRawImage[0], &frameDesc, imageType, pEncodedImage, &encodedImageSize);
        if (!API_SUCCESS(rc)) return rc;
    } else {
        pEncodedImage = &pRawImage[0];
        encodedImageSize = rawImageSize;
    }

    //
    // Step 4.
    //      Save the image to a file.
    size_t numBytesWritten;
    FILE* pFile;

    // Open our file for binary write
    pFile = fopen(fileName, "wb");
    if (NULL == pFile) return ApiOSServiceError;

    numBytesWritten = fwrite(&pEncodedImage[0], sizeof(char), encodedImageSize, pFile);

    fclose(pFile);

    return ((U32)numBytesWritten == encodedImageSize) ? ApiSuccess : ApiOSServiceError;
}

/***********************************************************************
 *  Private members
 */

PXL_RETURN_CODE PxLCamera::getFlags (ULONG feature, ULONG *flags)
{
    PXL_RETURN_CODE rc = ApiSuccess;
    ULONG  featureSize = 0;

    rc = PxLGetCameraFeatures (m_hCamera, feature, NULL, &featureSize);
    if (!API_SUCCESS(rc)) return rc;
    vector<BYTE> featureStore(featureSize);
    PCAMERA_FEATURES pFeatureInfo= (PCAMERA_FEATURES)&featureStore[0];
    rc = PxLGetCameraFeatures (m_hCamera, feature, pFeatureInfo, &featureSize);
    if (!API_SUCCESS(rc)) return rc;

    if (1 != pFeatureInfo->uNumberOfFeatures || NULL == pFeatureInfo->pFeatures) return ApiInvalidParameterError;

    *flags = pFeatureInfo->pFeatures->uFlags;

    return ApiSuccess;
}

bool PxLCamera::requiresStreamStop (ULONG feature)
{
    PXL_RETURN_CODE rc = ApiSuccess;
    ULONG  flags = 0;

    rc = getFlags (feature, &flags);
    if (!API_SUCCESS(rc)) return false;

    return ((flags & FEATURE_FLAG_PRESENCE) && !(flags & FEATURE_FLAG_SETTABLE_WHILE_STREAMING));
}

ULONG  PxLCamera::imageSize ()
{
    PXL_RETURN_CODE rc = ApiSuccess;

    float parms[4];     // reused for each feature query
    U32 roiWidth;
    U32 roiHeight;
    U32 pixelAddressingValue;       // integral factor by which the image is reduced
    U32 pixelFormat;
    float numPixels;
    U32 flags = FEATURE_FLAG_MANUAL;
    U32 numParams;

    assert(0 != m_hCamera);

    // Get region of interest (ROI)
    numParams = 4; // left, top, width, height
    rc = PxLGetFeature(m_hCamera, FEATURE_ROI, &flags, &numParams, &parms[0]);
    if (!API_SUCCESS(rc)) return 0;
    roiWidth    = (U32)parms[FEATURE_ROI_PARAM_WIDTH];
    roiHeight   = (U32)parms[FEATURE_ROI_PARAM_HEIGHT];

    // Query pixel addressing
    numParams = 2; // pixel addressing value, pixel addressing type (e.g. bin, average, ...)
    rc = PxLGetFeature(m_hCamera, FEATURE_PIXEL_ADDRESSING, &flags, &numParams, &parms[0]);
    if (!API_SUCCESS(rc)) return 0;
    pixelAddressingValue = (U32)parms[FEATURE_PIXEL_ADDRESSING_PARAM_VALUE];

    // We can calculate the number of pixels now.
    numPixels = (float)((roiWidth / pixelAddressingValue) * (roiHeight / pixelAddressingValue));

    // Knowing pixel format means we can determine how many bytes per pixel.
    numParams = 1;
    rc = PxLGetFeature(m_hCamera, FEATURE_PIXEL_FORMAT, &flags, &numParams, &parms[0]);
    if (!API_SUCCESS(rc)) return 0;
    pixelFormat = (U32)parms[0];

    return (U32) (numPixels * pixelSize (pixelFormat));
}

PXL_RETURN_CODE PxLCamera::getNextFrame (ULONG bufferSize, void*pFrame, FRAME_DESC* pFrameDesc)
{
    int numTries = 0;
    const int MAX_NUM_TRIES = 4;
    PXL_RETURN_CODE rc = ApiUnknownError;

    for(numTries = 0; numTries < MAX_NUM_TRIES; numTries++) {
        // Important that we set the frame desc size before each and every call to PxLGetNextFrame
        pFrameDesc->uSize = sizeof(FRAME_DESC);
        rc = PxLGetNextFrame(m_hCamera, bufferSize, pFrame, pFrameDesc);
        if (API_SUCCESS(rc)) {
            break;
        }
    }

    return rc;

}





