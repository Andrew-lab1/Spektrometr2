/***************************************************************************
 *
 *     File: camera.cpp
 *
 *     Description: Class definition for a very simple camera.
 */

#include <unistd.h>
#include <vector>
#include <memory>

#include "PixeLINKApi.h"
#include "camera.h"
#include "captureOEM.h"
#include "helpers.h"

using namespace std;

// Our app uses OneTime in favor of OnePush
#define FEATURE_FLAG_ONETIME FEATURE_FLAG_ONEPUSH

// define a macro that will conveniently interrupt the stream is needed to make a feature adjustment
#define STOP_STREAM_IF_REQUIRED(FEATURE)                                                    \
    std::auto_ptr<PxLInterruptStream> _temp_ss(NULL);                                             \
    if (requiresStreamStop(FEATURE))                                                     \
        _temp_ss = std::auto_ptr<PxLInterruptStream>(new PxLInterruptStream(this, STOP_STREAM));  \

extern "C" U32 PreviewWindowEvent (HANDLE hCamera, U32 event, LPVOID pdata);

static U32 PxLFrameCallback (
        HANDLE hCamera,
        LPVOID pFrameData,
        U32    uDataFormat,
        FRAME_DESC const * pFramedesc,
        LPVOID pContext);

/* ---------------------------------------------------------------------------
 * --   Member functions : public
 * ---------------------------------------------------------------------------
 */

PxLTriggerInfo::PxLTriggerInfo()
: m_enabled(false)
, m_mode(0)
, m_type(TRIGGER_TYPE_SOFTWARE)
, m_polarity(POLARITY_NEGATIVE)
, m_delay(0)
, m_number(1)
{}

PxLGpioInfo::PxLGpioInfo()
: m_enabled(false)
, m_mode(GPIO_MODE_NORMAL)  // All cameras with GPIO, support mode normal (1)
, m_polarity(POLARITY_NEGATIVE)
, m_param1(0)
, m_param2(0)
, m_param3(0)
{}

PxLCamera::PxLCamera (PCAMERA_ID_INFO pCameraIdInfo)
: m_ssUpdateFunc(NULL)
, m_serialNum(0)
, m_hCamera(NULL)
, m_streamState(STOP_STREAM)
, m_previewState(STOP_PREVIEW)
, m_pixelFormatInterpretation(HSV_AS_COLOR)
{
    PXL_RETURN_CODE rc = ApiSuccess;
    char  title[40];

    rc = PxLInitializeEx (pCameraIdInfo->CameraSerialNum, &m_hCamera, CAMERA_INITIALIZE_EX_FLAG_ISSUE_STREAM_STOP);
    //if (!API_SUCCESS(rc) && rc != ApiNoCameraError)
    if (!API_SUCCESS(rc))
    {
        throw PxLError(rc);
    }

    // Set the preview window to a fixed size.
    sprintf (title, "Preview - Camera %d", m_serialNum);
    rc = PxLSetPreviewSettings (m_hCamera, title, 0, 128, 128, 1024, 768);
    if (!API_SUCCESS(rc))
    {
        throw PxLError(rc);
    }

    m_serialNum = pCameraIdInfo->CameraSerialNum;
    m_cameraIdInfo = *pCameraIdInfo;

    // Create our poller object
    m_poller = new PxLFeaturePoller (1000);

}

PxLCamera::~PxLCamera()
{
    // Bugzilla.2179 - Stop the preview and stream if necessary.
    //     Don't bother setting state as we are destroying the object
    if (m_previewState != STOP_PREVIEW) PxLSetPreviewState(m_hCamera, STOP_PREVIEW, &m_previewHandle);
    if (m_streamState  != STOP_STREAM)  PxLSetStreamState (m_hCamera, STOP_STREAM);

    // Cancel the Sharpness score callback, if there is one
    if (m_ssUpdateFunc)
    {
        m_ssUpdateFunc = NULL;
        PxLSetCallback (m_hCamera, OVERLAY_FRAME, NULL, NULL);
    }

    // Cancel camera disconnect event handler
    PxLSetEventCallback (m_hCamera, EVENT_CAMERA_DISCONNECTED, NULL, NULL);

    // cancel the preview callback of there is one
    PxLSetCallback (m_hCamera, OVERLAY_PREVIEW, NULL, NULL);

    PxLUninitialize (m_hCamera);

    // destroy our poller
    delete m_poller;

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
    rc = PxLSetPreviewStateEx(m_hCamera, START_PREVIEW, &m_previewHandle, NULL, PreviewWindowEvent);
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

    // If we wanted, we can also pause the stream.  This will make the bus quieter, and
    // a little less load on the system.
    rc = PxLSetStreamState (m_hCamera, PAUSE_STREAM);
    if (!API_SUCCESS(rc))
    {
        PxLSetPreviewState (m_hCamera, m_previewState, &m_previewHandle);
        return rc;
    }
    m_streamState = PAUSE_STREAM;

    m_previewState = PAUSE_PREVIEW;

    return ApiSuccess;
}

// Suspend is very similar to pause, but the stream is actually stopped.  This function
// is useful if a quick adjustment needs to made (that requries the stream to be stopped)
// while previewing
PXL_RETURN_CODE PxLCamera::suspend()
{
    PXL_RETURN_CODE rc = ApiSuccess;

    rc = PxLSetPreviewState (m_hCamera, PAUSE_PREVIEW, &m_previewHandle);
    if (!API_SUCCESS(rc)) return rc;

    // We we wanted, we can also pause the stream.  This will make the bus quieter, and
    // a little less load on the system.
    rc = PxLSetStreamState (m_hCamera, STOP_STREAM);
    if (!API_SUCCESS(rc))
    {
        PxLSetPreviewState (m_hCamera, m_previewState, &m_previewHandle);
        return rc;
    }
    m_streamState = STOP_STREAM;

    m_previewState = PAUSE_PREVIEW;

    return ApiSuccess;
}

PXL_RETURN_CODE PxLCamera::stop()
{
    PXL_RETURN_CODE rc = ApiSuccess;

    rc = PxLSetPreviewState(m_hCamera, STOP_PREVIEW, &m_previewHandle);
    if (!API_SUCCESS(rc)) return rc;

    rc = PxLSetStreamState (m_hCamera, STOP_STREAM);
    if (!API_SUCCESS(rc))
    {
        PxLSetPreviewState(m_hCamera, m_previewState, &m_previewHandle);
        return rc;
    }
    m_previewState = STOP_PREVIEW;
    m_streamState = STOP_STREAM;

    return rc;
}

PXL_RETURN_CODE PxLCamera::pausePreview()
{
    PXL_RETURN_CODE rc = ApiSuccess;

    if (m_previewState == PAUSE_PREVIEW) return ApiSuccess;

    rc = PxLSetPreviewState(m_hCamera, PAUSE_PREVIEW, &m_previewHandle);
    if (!API_SUCCESS(rc)) return rc;

    m_previewState = PAUSE_PREVIEW;
    return rc;
}

PXL_RETURN_CODE PxLCamera::playPreview()
{
    PXL_RETURN_CODE rc = ApiSuccess;

    if (m_previewState == START_PREVIEW) return ApiSuccess;

    rc = PxLSetPreviewStateEx(m_hCamera, START_PREVIEW, &m_previewHandle, NULL, PreviewWindowEvent);
    if (!API_SUCCESS(rc)) return rc;

    m_previewState = START_PREVIEW;
    return rc;
}

PXL_RETURN_CODE PxLCamera::stopStream()
{
    PXL_RETURN_CODE rc = ApiSuccess;

    if (m_streamState == STOP_STREAM) return ApiSuccess;

    rc = PxLSetStreamState (m_hCamera, STOP_STREAM);
    if (!API_SUCCESS(rc)) return rc;
    m_streamState = STOP_STREAM;

    return ApiSuccess;
}

PXL_RETURN_CODE PxLCamera::startStream()
{
    PXL_RETURN_CODE rc = ApiSuccess;

    if (m_streamState == START_STREAM) return ApiSuccess;

    rc = PxLSetStreamState (m_hCamera, START_STREAM);
    if (!API_SUCCESS(rc)) return rc;
    m_streamState = START_STREAM;

    return ApiSuccess;
}

PXL_RETURN_CODE PxLCamera::pauseStream()
{
    PXL_RETURN_CODE rc = ApiSuccess;

    rc = PxLSetStreamState (m_hCamera, PAUSE_STREAM);
    if (!API_SUCCESS(rc)) return rc;
    m_streamState = START_STREAM;

    return ApiSuccess;
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

bool PxLCamera::supportedInCamera (ULONG feature)
{
    PXL_RETURN_CODE rc = ApiSuccess;
    ULONG  flags = 0;

    rc = getFlags (feature, &flags);
    if (!API_SUCCESS(rc)) return false;

    return (IS_FEATURE_SUPPORTED(flags) && !((flags) & FEATURE_FLAG_EMULATION));
}

bool PxLCamera::enabled (ULONG feature)
{
    PXL_RETURN_CODE rc = ApiSuccess;
    ULONG  flags = 0;
    float featureValues[10]; // This is large enough for any feature
    ULONG numParams = 10;

    rc = PxLGetFeature (m_hCamera, feature, &flags, &numParams, &featureValues[0]);
    if (!API_SUCCESS(rc)) return false;

    return (IS_FEATURE_ENABLED(flags));
}

bool PxLCamera::manualSupported (ULONG feature)
{
    PXL_RETURN_CODE rc = ApiSuccess;
    ULONG  flags = 0;

    rc = getFlags (feature, &flags);
    if (!API_SUCCESS(rc)) return false;

    return (0 != (flags & FEATURE_FLAG_MANUAL));
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

bool PxLCamera::settable (ULONG feature)
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

    for (int i = 0; i < (int)pFeatureInfo->pFeatures->uNumberOfParameters; i++)
    {
        if (pFeatureInfo->pFeatures->pParams[i].fMinValue !=
            pFeatureInfo->pFeatures->pParams[i].fMaxValue) return true;
    }

    // If we made it this far, all parameters indicate the same min and max value --
    // the feature is read only

    return false;
}

ULONG PxLCamera::numParametersSupported (ULONG feature)
{
    ULONG  featureSize = 0;

    PxLGetCameraFeatures (m_hCamera, feature, NULL, &featureSize);

    return (featureSize - (sizeof (CAMERA_FEATURES) +
                           sizeof (CAMERA_FEATURE)))/sizeof(FEATURE_PARAM); // Will be '0' on error
}

PXL_RETURN_CODE PxLCamera::disable (ULONG feature)
{
    PXL_RETURN_CODE rc = ApiSuccess;
    float value;
    ULONG flags = 0;
    ULONG numParameters = 1;

    flags = FEATURE_FLAG_MANUAL | FEATURE_FLAG_OFF;

    STOP_STREAM_IF_REQUIRED(feature);

    rc = PxLSetFeature (m_hCamera, feature, flags, numParameters, &value);

    return rc;
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

// This is used to get the second value of a feature, that is dependant on the first value.
//      Useful for a feature like FEATURE_COMPRESSION
PXL_RETURN_CODE PxLCamera::get2ndValue (ULONG feature, float firstValue, float* secondValue)
{
    PXL_RETURN_CODE rc = ApiSuccess;
    float featureValue[2];
    ULONG flags;
    ULONG numParams = 2;

    featureValue[0] = firstValue;
    featureValue[1] = *secondValue;
    rc = PxLGetFeature (m_hCamera, feature, &flags, &numParams, featureValue);
    if (!API_SUCCESS(rc)) return rc;

    *secondValue = featureValue[1];

    return ApiSuccess;
}

PXL_RETURN_CODE PxLCamera::setValue (ULONG feature, float value)
{
    PXL_RETURN_CODE rc = ApiSuccess;

    STOP_STREAM_IF_REQUIRED(feature);

    rc = PxLSetFeature (m_hCamera, feature, FEATURE_FLAG_MANUAL, 1, &value);

    return rc;
}

// This is used to set the second value of a feature, that is dependant on the first value.
//      Useful for a feature like FEATURE_COMPRESSION
PXL_RETURN_CODE PxLCamera::set2ndValue (ULONG feature, float firstValue, float secondValue)
{
    PXL_RETURN_CODE rc = ApiSuccess;
    float featureValue[2];

    STOP_STREAM_IF_REQUIRED(feature);

    featureValue[0] = firstValue;
    featureValue[1] = secondValue;
    rc = PxLSetFeature (m_hCamera, feature, FEATURE_FLAG_MANUAL, 2, featureValue);

    return rc;
}

PXL_RETURN_CODE PxLCamera::getOnetimeAuto (ULONG feature, bool* stillRunning)
{
    PXL_RETURN_CODE rc = ApiSuccess;
    float featureValue[3];
    ULONG flags;
    ULONG numParams = (feature == FEATURE_WHITE_SHADING ? 3 : 1);

    rc = PxLGetFeature (m_hCamera, feature, &flags, &numParams, &featureValue[0]);
    if (!API_SUCCESS(rc)) return rc;

    *stillRunning = (0 != (flags & FEATURE_FLAG_ONETIME));

    return ApiSuccess;
}


PXL_RETURN_CODE PxLCamera::setOnetimeAuto (ULONG feature, bool enable)
{
    PXL_RETURN_CODE rc = ApiSuccess;
    float value[3];
    ULONG flags = 0;
    ULONG numParameters = (feature == FEATURE_WHITE_SHADING ? 3 : 1);

    if (! enable)
    {
        // We are cancelling onetime auto adjustment, and restoring manual adjustment.
        // When we set the feature (to turn off onetime), we have to set the feature to
        // 'something' -- so read the current value so that we can use it.
        rc = PxLGetFeature (m_hCamera, feature, &flags, &numParameters, &value[0]);
        if (!API_SUCCESS(rc)) return rc;
    }

    flags = enable ? FEATURE_FLAG_ONETIME : FEATURE_FLAG_MANUAL;

    rc = PxLSetFeature (m_hCamera, feature, flags, numParameters, &value[0]);

    return rc;
}

// Initiates a one time auto operation, specifying the limits to use
PXL_RETURN_CODE PxLCamera::setOnetimeAuto (ULONG feature, bool enable, float min, float max)
{
    PXL_RETURN_CODE rc = ApiSuccess;
    float value[3];
    ULONG flags = 0;
    ULONG numParameters = 3;

    if (! enable)
    {
        // We are cancelling onetime auto adjustment, and restoring manual adjustment.
        // When we set the feature (to turn off onetime), we have to set the feature to
        // 'something' -- so read the current value so that we can use it.
        rc = PxLGetFeature (m_hCamera, feature, &flags, &numParameters, &value[0]);
        if (!API_SUCCESS(rc)) return rc;
    }

    if (feature == FEATURE_FOCUS)
    {
       // This feature is rather unique, in that it 'reuses' the first parameter as either a focus
       // value (on Gets) or as the min value (on Sets).
        numParameters = 2;
        value[0] = min;
        value[1] = max;
    } else {
        numParameters = 3;
        value[1] = min;
        value[2] = max;
    }

    flags = enable ? FEATURE_FLAG_ONETIME : FEATURE_FLAG_MANUAL;

    rc = PxLSetFeature (m_hCamera, feature, flags, numParameters, &value[0]);

    return rc;
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

    if (! enable)
    {
        // We are disabling continuous auto adjustment, and restoring manual adjustment.
        // When we set the feature (to turn off continuous), we have to set the feature to
        // 'something' -- so read the current value so that we can use it.
        rc = PxLGetFeature (m_hCamera, feature, &flags, &numParameters, &value);
        if (!API_SUCCESS(rc)) return rc;
    }

    flags = enable ? FEATURE_FLAG_AUTO : FEATURE_FLAG_MANUAL;

    STOP_STREAM_IF_REQUIRED(feature);

    rc = PxLSetFeature (m_hCamera, feature, flags, numParameters, &value);

    return rc;
}

// Initiates a one time auto operation, specifying the limits to use (2nd and 3rd parameters
PXL_RETURN_CODE PxLCamera::setContinuousAuto (ULONG feature, bool enable, float min, float max)
{
    PXL_RETURN_CODE rc = ApiSuccess;
    float values[3];
    ULONG flags = 0;
    ULONG numParameters = 3;

    values[1] = min;
    values[2] = max;

    if (! enable)
    {
        // We are disabling continuous auto adjustment, and restoring manual adjustment.
        // When we set the feature (to turn off continuous), we have to set the feature to
        // 'something' -- so read the current value so that we can use it.
        rc = PxLGetFeature (m_hCamera, feature, &flags, &numParameters, values);
        if (!API_SUCCESS(rc)) return rc;
    }

    flags = enable ? FEATURE_FLAG_AUTO : FEATURE_FLAG_MANUAL;

    STOP_STREAM_IF_REQUIRED(feature);

    rc = PxLSetFeature (m_hCamera, feature, flags, numParameters, values);

    return rc;
}

//
// Gets the limits used for auto operations
//    Assumptions
//      - The camera supports auto limits
//      - the auto limits are the second and third parameters of the feature
PXL_RETURN_CODE PxLCamera::getAutoLimits (ULONG feature, float* min, float* max)
{
    PXL_RETURN_CODE rc = ApiSuccess;
    float featureValue[3];
    ULONG flags;
    ULONG numParams = 3;

    rc = PxLGetFeature (m_hCamera, feature, &flags, &numParams, featureValue);
    if (!API_SUCCESS(rc)) return rc;

    *min = featureValue[1];
    *max = featureValue[2];

    return ApiSuccess;
}

// Returns true if the camera is currently in a triggered state
bool PxLCamera::triggering ()
{
    PXL_RETURN_CODE rc = ApiSuccess;
    float value[5];
    ULONG flags = 0;
    ULONG numParameters = 5;

    rc = PxLGetFeature (m_hCamera, FEATURE_TRIGGER, &flags, &numParameters, value);
    if (API_SUCCESS(rc))
    {
        return ((flags & FEATURE_FLAG_OFF) == 0);
    }

    return false;
}

// Returns true if the camera is currently in a hardware triggered state
bool PxLCamera::hardwareTriggering ()
{
    PXL_RETURN_CODE rc = ApiSuccess;
    float value[5];
    ULONG flags = 0;
    ULONG numParameters = 5;

    rc = PxLGetFeature (m_hCamera, FEATURE_TRIGGER, &flags, &numParameters, value);
    if (API_SUCCESS(rc))
    {
        return ((flags & FEATURE_FLAG_OFF) == 0 && IS_HARDWARE_TRIGGER(value[1]));
    }

    return false;
}

PXL_RETURN_CODE PxLCamera::getPixelAddressRange (float* minMode, float* maxMode, float* minValue, float* maxValue, bool* asymmetric)
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

    // Supports asymmetric pixel addressing if it supports more than 2 parameters
    *asymmetric = pFeatureInfo->pFeatures->uNumberOfParameters > 2;

    return ApiSuccess;
}

PXL_RETURN_CODE PxLCamera::getPixelAddressValues (float* mode, float* valueX, float* valueY)
{
    PXL_RETURN_CODE rc = ApiSuccess;
    ULONG numParams = 4;
    float featureValue[numParams];
    ULONG flags;

    rc = PxLGetFeature (m_hCamera, FEATURE_PIXEL_ADDRESSING, &flags, &numParams, featureValue);
    if (!API_SUCCESS(rc)) return rc;

    *mode = featureValue[FEATURE_PIXEL_ADDRESSING_PARAM_MODE];
    if (numParams > 2)
    {
        // supports asymmetric Pixel addressing
        *valueX = featureValue[FEATURE_PIXEL_ADDRESSING_PARAM_X_VALUE];
        *valueY = featureValue[FEATURE_PIXEL_ADDRESSING_PARAM_Y_VALUE];
    } else {
        *valueX = featureValue[FEATURE_PIXEL_ADDRESSING_PARAM_VALUE];
        *valueY = featureValue[FEATURE_PIXEL_ADDRESSING_PARAM_VALUE];
    }

    return ApiSuccess;
}

//
// NOTE:
//      If valueX != valueY, then the invoker should have made sure that the camera supports
//      asymmetric PA first
PXL_RETURN_CODE PxLCamera::setPixelAddressValues (float mode, float valueX, float valueY)
{
    PXL_RETURN_CODE rc = ApiSuccess;
    ULONG numParams = 4;
    float featureValue[numParams];

    STOP_STREAM_IF_REQUIRED(FEATURE_PIXEL_ADDRESSING);

    featureValue[FEATURE_PIXEL_ADDRESSING_PARAM_MODE] = mode;
    featureValue[FEATURE_PIXEL_ADDRESSING_PARAM_VALUE] = valueX;
    featureValue[FEATURE_PIXEL_ADDRESSING_PARAM_X_VALUE] = valueX;
    featureValue[FEATURE_PIXEL_ADDRESSING_PARAM_Y_VALUE] = valueY;

    rc = PxLSetFeature (m_hCamera, FEATURE_PIXEL_ADDRESSING, FEATURE_FLAG_MANUAL, numParams, featureValue);

    return rc;
}

PXL_RETURN_CODE PxLCamera::getRoiRange (ROI_TYPE type, PXL_ROI* minRoi, PXL_ROI* maxRoi, bool ignoreRotate, float* maxSs)
{
    PXL_RETURN_CODE rc = ApiSuccess;
    ULONG  featureSize = 0;

    //
    // Step 1.
    //      Figure out what ROI we are interested in
    U32 feature;
    switch (type)
    {
    case SharpnessScoreRoi:
        feature = FEATURE_SHARPNESS_SCORE;
        break;
    case AutoRoi:
        feature = FEATURE_AUTO_ROI;
        break;
    case FrameRoi:
    default:
        feature = FEATURE_ROI;
        break;
    }

    //
    // Step 2.
    //      Get the requested ROI limit information
    rc = PxLGetCameraFeatures (m_hCamera, feature, NULL, &featureSize);
    if (!API_SUCCESS(rc)) return rc;
    vector<BYTE> featureStore(featureSize);
    PCAMERA_FEATURES pFeatureInfo= (PCAMERA_FEATURES)&featureStore[0];
    rc = PxLGetCameraFeatures (m_hCamera, feature, pFeatureInfo, &featureSize);
    if (!API_SUCCESS(rc)) return rc;

    if (1 != pFeatureInfo->uNumberOfFeatures ||
        NULL == pFeatureInfo->pFeatures ||
        NULL == pFeatureInfo->pFeatures->pParams ||
        pFeatureInfo->pFeatures->uNumberOfParameters < 4) return ApiInvalidParameterError;

    minRoi->m_height = (int)pFeatureInfo->pFeatures->pParams[FEATURE_ROI_PARAM_HEIGHT].fMinValue;
    minRoi->m_width = (int)pFeatureInfo->pFeatures->pParams[FEATURE_ROI_PARAM_WIDTH].fMinValue;
    minRoi->m_offsetX = (int)pFeatureInfo->pFeatures->pParams[FEATURE_ROI_PARAM_LEFT].fMinValue;
    minRoi->m_offsetY = (int)pFeatureInfo->pFeatures->pParams[FEATURE_ROI_PARAM_TOP].fMinValue;

    maxRoi->m_height = (int)pFeatureInfo->pFeatures->pParams[FEATURE_ROI_PARAM_HEIGHT].fMaxValue;
    maxRoi->m_width = (int)pFeatureInfo->pFeatures->pParams[FEATURE_ROI_PARAM_WIDTH].fMaxValue;
    maxRoi->m_offsetX = (int)pFeatureInfo->pFeatures->pParams[FEATURE_ROI_PARAM_LEFT].fMaxValue;
    maxRoi->m_offsetY = (int)pFeatureInfo->pFeatures->pParams[FEATURE_ROI_PARAM_TOP].fMaxValue;

    //
    //  Step 3.
    //      figure out if we rotating the image
    if (!ignoreRotate)
    {
        float rotate = 0.0;
        getValue (FEATURE_ROTATE, &rotate);

        minRoi->rotateClockwise((int)rotate);
        maxRoi->rotateClockwise((int)rotate);
    }

    if (maxSs != NULL && type == SharpnessScoreRoi)
    {
        *maxSs = pFeatureInfo->pFeatures->pParams[FEATURE_SHARPNESS_SCORE_MAX_VALUE].fMaxValue;
    }

	return ApiSuccess;
}

PXL_RETURN_CODE PxLCamera::getRoiValue (ROI_TYPE type, PXL_ROI* roi, bool ignoreTransforms)
{
    PXL_RETURN_CODE rc = ApiSuccess;
    ULONG numParams = 5;
    float featureValue[numParams];
    ULONG flags;

    //
    // Step 1.
    //      Figure out what ROI we are interested in
    U32 feature;
    switch (type)
    {
    case SharpnessScoreRoi:
        feature = FEATURE_SHARPNESS_SCORE;
        break;
    case AutoRoi:
        feature = FEATURE_AUTO_ROI;
        break;
    case FrameRoi:
    default:
        feature = FEATURE_ROI;
        break;
    }

    //
    // Step 2
    //      Get the camera's ROI
    rc = PxLGetFeature (m_hCamera, feature, &flags, &numParams, featureValue);
    if (!API_SUCCESS(rc)) return rc;

    roi->m_width = (int)featureValue[FEATURE_ROI_PARAM_WIDTH];
    roi->m_height = (int)featureValue[FEATURE_ROI_PARAM_HEIGHT];
    roi->m_offsetX = (int)featureValue[FEATURE_ROI_PARAM_LEFT];
    roi->m_offsetY = (int)featureValue[FEATURE_ROI_PARAM_TOP];

    //
    // Step 3
    //      Determine if the camera is flipping or rotating
    if (! ignoreTransforms)
    {
        bool hFlip = false;
        bool vFlip = false;
        getFlip (&hFlip, &vFlip);

        float rotate = 0.0;
        getValue (FEATURE_ROTATE, &rotate);

        if (hFlip || vFlip || rotate != 0.0)
        {
            //
            // Step 4
            //      We need to transpose the ROI values to accommodate the
            //      flip or rotate.  but first, we need to know the maxRoi
            PXL_ROI minRoi;
            PXL_ROI maxRoi;
            if (type == FrameRoi)
            {
                rc = getRoiRange (type, &minRoi, &maxRoi, true); // Don't transpose the max
            } else {
                // The other types are a subset of the ROI
                rc = getRoiValue (FrameRoi, &maxRoi, true); // Don't transpose the max
            }
            maxRoi.m_offsetX = maxRoi.m_offsetY = 0;  // Offests are 0 for the max
            if (API_SUCCESS(rc))
            {
                // note that maxRoi has NOT already been transposed
                roi->rotateClockwise ((int)rotate, maxRoi);
                // flip need the transposed maxRoi
                maxRoi.rotateClockwise((int)rotate);
                if (hFlip) roi->flipHorizontal(maxRoi);
                if (vFlip) roi->flipVertical(maxRoi);
            }
        }
    }

	return rc;
}

PXL_RETURN_CODE PxLCamera::setRoiValue (ROI_TYPE type, PXL_ROI &roi, bool off)
{
    PXL_RETURN_CODE rc = ApiSuccess;
    ULONG numParams = 5;
    float featureValue[numParams];
    PXL_ROI adjustedRoi = roi;

    //
    // Step 1.
    //      Figure out what ROI we are interested in
    U32 feature;
    switch (type)
    {
    case SharpnessScoreRoi:
        feature = FEATURE_SHARPNESS_SCORE;
        break;
    case AutoRoi:
        feature = FEATURE_AUTO_ROI;
        break;
    case FrameRoi:
    default:
        feature = FEATURE_ROI;
        break;
    }

    // Step 2
    //      Stop the stream if necessary
    STOP_STREAM_IF_REQUIRED(feature);

    //
    // Step 3
    //      figure out if we need to transpose the ROI to deal
    //      with flip and rotate
    bool hFlip = false;
    bool vFlip = false;
    getFlip (&hFlip, &vFlip);

    float rotate = 0.0;
    getValue (FEATURE_ROTATE, &rotate);

    if (hFlip || vFlip || rotate != 0.0)
    {
        //
        // Step 4
        //      We need to transpose the ROI values to accommodate the
        //      flip or rotate.  but first, we need to know the maxRoi
        PXL_ROI minRoi;
        PXL_ROI maxRoi;
        rc = getRoiRange (type, &minRoi, &maxRoi, true); // don't transpose the ROIs
        maxRoi.m_offsetX = maxRoi.m_offsetY = 0;  // Offests are 0 for ffov
        PXL_ROI rotatedMaxRoi = maxRoi;
        rotatedMaxRoi.rotateCounterClockwise((int)rotate);
        if (API_SUCCESS(rc))
        {
            // flip need the transposed maxRoi
            if (hFlip) adjustedRoi.flipHorizontal(rotatedMaxRoi);
            if (vFlip) adjustedRoi.flipVertical(rotatedMaxRoi);
            // note that maxRoi has NOT already been transposed
            adjustedRoi.rotateCounterClockwise ((int)rotate, maxRoi);
        }
    }

    featureValue[FEATURE_ROI_PARAM_WIDTH] = (float)adjustedRoi.m_width;
    featureValue[FEATURE_ROI_PARAM_HEIGHT] = (float)adjustedRoi.m_height;
    featureValue[FEATURE_ROI_PARAM_LEFT] = (float)adjustedRoi.m_offsetX;
    featureValue[FEATURE_ROI_PARAM_TOP] = (float)adjustedRoi.m_offsetY;

    //
    // Step 5
    //      Set the feature.  By default, we will set the feature as enabled, but give the option
    //      for the user to leave the feature disabled, but to change the parameters none-the-less
    if (API_SUCCESS(rc))
    {
        if (off)
        {
            rc = PxLSetFeature (m_hCamera, feature, FEATURE_FLAG_OFF, numParams, featureValue);
        } else {
            rc = PxLSetFeature (m_hCamera, feature, FEATURE_FLAG_MANUAL, numParams, featureValue);
        }
    }

    return rc;
}

PXL_RETURN_CODE PxLCamera::getTriggerRange (float* minMode, float* maxMode, float* minType, float* maxType)
{
    PXL_RETURN_CODE rc = ApiSuccess;
    ULONG  featureSize = 0;

    rc = PxLGetCameraFeatures (m_hCamera, FEATURE_TRIGGER, NULL, &featureSize);
    if (!API_SUCCESS(rc)) return rc;
    vector<BYTE> featureStore(featureSize);
    PCAMERA_FEATURES pFeatureInfo= (PCAMERA_FEATURES)&featureStore[0];
    rc = PxLGetCameraFeatures (m_hCamera, FEATURE_TRIGGER, pFeatureInfo, &featureSize);
    if (!API_SUCCESS(rc)) return rc;

    if (1 != pFeatureInfo->uNumberOfFeatures ||
        NULL == pFeatureInfo->pFeatures ||
        NULL == pFeatureInfo->pFeatures->pParams ||
        pFeatureInfo->pFeatures->uNumberOfParameters < 2) return ApiInvalidParameterError;

    *minMode = pFeatureInfo->pFeatures->pParams[FEATURE_TRIGGER_PARAM_MODE].fMinValue;
    *maxMode = pFeatureInfo->pFeatures->pParams[FEATURE_TRIGGER_PARAM_MODE].fMaxValue;
    *minType = pFeatureInfo->pFeatures->pParams[FEATURE_TRIGGER_PARAM_TYPE].fMinValue;
    *maxType = pFeatureInfo->pFeatures->pParams[FEATURE_TRIGGER_PARAM_TYPE].fMaxValue;

    return ApiSuccess;
}

PXL_RETURN_CODE PxLCamera::getTriggerValue (PxLTriggerInfo& trig)
{
    PXL_RETURN_CODE rc = ApiSuccess;
    float values[5];
    ULONG flags = 0;
    ULONG numParameters = 5;

    rc = PxLGetFeature (m_hCamera, FEATURE_TRIGGER, &flags, &numParameters, values);
    if (! API_SUCCESS(rc)) return rc;

    PxLTriggerInfo currentTrig;

    currentTrig.m_enabled= (flags & FEATURE_FLAG_OFF) == 0;
    currentTrig.m_type = values[FEATURE_TRIGGER_PARAM_TYPE];

    if (IS_HARDWARE_TRIGGER(currentTrig.m_type))
    {
        currentTrig.m_mode = values[FEATURE_TRIGGER_PARAM_MODE];
        currentTrig.m_polarity = values[FEATURE_TRIGGER_PARAM_POLARITY];
        currentTrig.m_delay = values[FEATURE_TRIGGER_PARAM_DELAY];
        if (currentTrig.m_mode == 14)
        {
            currentTrig.m_number = values[FEATURE_TRIGGER_PARAM_NUMBER];
        }
    }

    trig = currentTrig;

    return ApiSuccess;
}

PXL_RETURN_CODE PxLCamera::setTriggerValue (PxLTriggerInfo& trig)
{
    float values[5];
    ULONG flags = 0;
    ULONG numParameters = 5;

    flags = trig.m_enabled ? FEATURE_FLAG_MANUAL : FEATURE_FLAG_OFF;

    values [FEATURE_TRIGGER_PARAM_MODE] = trig.m_mode;
    values [FEATURE_TRIGGER_PARAM_TYPE] = trig.m_type;
    values [FEATURE_TRIGGER_PARAM_POLARITY] = trig.m_polarity;
    values [FEATURE_TRIGGER_PARAM_DELAY] = trig.m_delay;
    values [FEATURE_TRIGGER_PARAM_NUMBER] = trig.m_number;

    return PxLSetFeature (m_hCamera, FEATURE_TRIGGER, flags, numParameters, values);
}

PXL_RETURN_CODE PxLCamera::sendActionCommand (U32 action, double actionDelay)
{
    PXL_RETURN_CODE rc = ApiSuccess;
    double currentTime = 0.0;

    rc = PxLGetCurrentTimestamp (m_hCamera, &currentTime);
    if (!API_SUCCESS(rc)) return rc;

    return PxLSetActions (action, currentTime + actionDelay);
}

PXL_RETURN_CODE PxLCamera::getGpioRange (int* numGpios, float* minMode, float* maxMode)
{
    PXL_RETURN_CODE rc = ApiSuccess;
    ULONG  featureSize = 0;

    rc = PxLGetCameraFeatures (m_hCamera, FEATURE_GPIO, NULL, &featureSize);
    if (!API_SUCCESS(rc)) return rc;
    vector<BYTE> featureStore(featureSize);
    PCAMERA_FEATURES pFeatureInfo= (PCAMERA_FEATURES)&featureStore[0];
    rc = PxLGetCameraFeatures (m_hCamera, FEATURE_GPIO, pFeatureInfo, &featureSize);
    if (!API_SUCCESS(rc)) return rc;

    if (1 != pFeatureInfo->uNumberOfFeatures ||
        NULL == pFeatureInfo->pFeatures ||
        NULL == pFeatureInfo->pFeatures->pParams ||
        pFeatureInfo->pFeatures->uNumberOfParameters < 2) return ApiInvalidParameterError;

    // The Camera/API uses 1-based indices, so the max value is the same as the count
    *numGpios = (int)pFeatureInfo->pFeatures->pParams[FEATURE_GPIO_PARAM_GPIO_INDEX].fMaxValue;

    *minMode = pFeatureInfo->pFeatures->pParams[FEATURE_GPIO_PARAM_MODE].fMinValue;
    *maxMode = pFeatureInfo->pFeatures->pParams[FEATURE_GPIO_PARAM_MODE].fMaxValue;

    return ApiSuccess;
}

// NOTE:
//   - gpioNum value is 0-based
PXL_RETURN_CODE PxLCamera::getGpioValue (int gpioNum, PxLGpioInfo& info)
{
    PXL_RETURN_CODE rc = ApiSuccess;
    float values[6];
    ULONG flags = 0;
    ULONG numParameters = 6;

    values[FEATURE_GPIO_PARAM_GPIO_INDEX] = (float)gpioNum+1;
    rc = PxLGetFeature (m_hCamera, FEATURE_GPIO, &flags, &numParameters, values);
    if (! API_SUCCESS(rc)) return rc;

    info.m_enabled= (flags & FEATURE_FLAG_OFF) == 0;

    info.m_mode = values[FEATURE_GPIO_PARAM_MODE];
    info.m_polarity = values[FEATURE_GPIO_PARAM_POLARITY];
    info.m_param1 = values[FEATURE_GPIO_PARAM_PARAM_1];
    info.m_param2 = values[FEATURE_GPIO_PARAM_PARAM_2];
    info.m_param3 = values[FEATURE_GPIO_PARAM_PARAM_3];

    return ApiSuccess;
}

// NOTE:
//   - gpioNum value is 0-based
PXL_RETURN_CODE PxLCamera::setGpioValue (int gpioNum, PxLGpioInfo& info)
{
    float values[6];
    ULONG flags = 0;
    ULONG numParameters = 6;

    flags = info.m_enabled ? FEATURE_FLAG_MANUAL : FEATURE_FLAG_OFF;

    values [FEATURE_GPIO_PARAM_GPIO_INDEX] = (float)gpioNum+1;
    values [FEATURE_GPIO_PARAM_MODE] = info.m_mode;
    values [FEATURE_TRIGGER_PARAM_POLARITY] = info.m_polarity;
    values [FEATURE_GPIO_PARAM_PARAM_1] = info.m_param1;
    values [FEATURE_GPIO_PARAM_PARAM_2] = info.m_param2;
    values [FEATURE_GPIO_PARAM_PARAM_3] = info.m_param3;

    return PxLSetFeature (m_hCamera, FEATURE_GPIO, flags, numParameters, values);
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
    *vertical =   featureValues[1] != 0.0f;

    return ApiSuccess;
}

PXL_RETURN_CODE PxLCamera::setFlip (bool horizontal, bool vertical)
{
    PXL_RETURN_CODE rc = ApiSuccess;

    STOP_STREAM_IF_REQUIRED(FEATURE_FLIP);

    float featureValues[2];
    featureValues[0] = horizontal ? 1.0f : 0.0f;;
    featureValues[1] = vertical ? 1.0f : 0.0f;

    rc = PxLSetFeature (m_hCamera, FEATURE_FLIP, FEATURE_FLAG_MANUAL, 2, featureValues);

    return rc;
}

PXL_RETURN_CODE PxLCamera::getPtpStatus (int* status, int* mode)
{
    PXL_RETURN_CODE rc = ApiSuccess;
    ULONG numParams = 4;
    float featureValue[numParams];
    ULONG flags;

    rc = PxLGetFeature (m_hCamera, FEATURE_PTP, &flags, &numParams, featureValue);
    if (!API_SUCCESS(rc)) return rc;

    *status = (int)featureValue[FEATURE_PTP_PARAM_STATUS];
    *mode = (int)featureValue[FEATURE_PTP_PARAM_MODE];

    return ApiSuccess;
}

PXL_RETURN_CODE PxLCamera::setPtpMode (int value)
{
    PXL_RETURN_CODE rc = ApiSuccess;
    ULONG numParams = 4;
    float featureValue[numParams];

    STOP_STREAM_IF_REQUIRED(FEATURE_PTP);

    featureValue[FEATURE_PTP_PARAM_MODE] = (float)value;
    // On Sets, all other params are don't care

    rc = PxLSetFeature (m_hCamera,
                        FEATURE_PTP,
                        value == FEATURE_PTP_MODE_DISABLED ? FEATURE_FLAG_OFF : FEATURE_FLAG_MANUAL,
                        numParams,
                        featureValue);

    return rc;
}

PXL_RETURN_CODE PxLCamera::getCurrentTimestamp (double* value)
{

    return PxLGetCurrentTimestamp (m_hCamera, value);
}

//
// Gets the alpha value currently being used for the specified alpha pixle format.
//   If the camera is not currently using an alpha pixel format, then returns
//   alphaPixelFormat == -1
PXL_RETURN_CODE PxLCamera::getAlpha (int* alphaPixelFormat, ULONG* alphaValue)
{
    PXL_RETURN_CODE rc = ApiSuccess;
    float featureValues[2];
    ULONG flags;
    ULONG numParams = 2;

    //Assume failure --
    *alphaPixelFormat = -1;
    *alphaValue = 0;

    rc = PxLGetFeature (m_hCamera, FEATURE_PIXEL_FORMAT, &flags, &numParams, featureValues);
    if (!API_SUCCESS(rc)) return rc;

    if (featureValues[0] == PIXEL_FORMAT_RGBA ||
        featureValues[0] == PIXEL_FORMAT_BGRA ||
        featureValues[0] == PIXEL_FORMAT_ARGB ||
        featureValues[0] == PIXEL_FORMAT_ABGR)
    {
        *alphaPixelFormat  = (int)featureValues[0];
        *alphaValue = (ULONG)featureValues[1];
    }

    return ApiSuccess;
}

//
// Sets the alpha value and Alpha pixel format.  If you want to set the pixel format to a non
//   alphs pixel format, use SetFeature
PXL_RETURN_CODE PxLCamera::setAlpha (int alphaPixelFormat, ULONG alphaValue)
{
    PXL_RETURN_CODE rc = ApiSuccess;
    float featureValues[2];
    ULONG flags = FEATURE_FLAG_MANUAL;
    ULONG numParams = 2;


    if (alphaPixelFormat != PIXEL_FORMAT_RGBA &&
        alphaPixelFormat != PIXEL_FORMAT_BGRA &&
        alphaPixelFormat != PIXEL_FORMAT_ARGB &&
        alphaPixelFormat != PIXEL_FORMAT_ABGR)
    {
        return ApiInvalidParameterError;
    }

    featureValues[0] = (float)alphaPixelFormat;
    featureValues[1] = (float)alphaValue;
    rc = PxLSetFeature (m_hCamera, FEATURE_PIXEL_FORMAT, flags, numParams, featureValues);

    return rc;
}

// returns the size of images from the camera (in bytes)
ULONG  PxLCamera::imageSizeInPixels ()
{
    PXL_RETURN_CODE rc = ApiSuccess;

    float parms[4];     // reused for each feature query
    U32 roiWidth;
    U32 roiHeight;
    U32 pixelAddressingXValue = 1;       // integral factor by which the image width is reduced
    U32 pixelAddressingYValue = 1;       // integral factor by which the image height is reduced
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

    // Determine if the image is interleaved, and double the width if so.
    numParams = 1;
    rc = PxLGetFeature(m_hCamera, FEATURE_GAIN_HDR, &flags, &numParams, parms);
    if (API_SUCCESS (rc) && parms[0] == FEATURE_GAIN_HDR_MODE_INTERLEAVED) roiWidth *= 2;

    // Query pixel addressing
    numParams = 4; // pixel addressing value, pixel addressing type (e.g. bin, average, ...)
    rc = PxLGetFeature(m_hCamera, FEATURE_PIXEL_ADDRESSING, &flags, &numParams, &parms[0]);
    if (API_SUCCESS(rc))
    {
        if (numParams < 4)
        {
            pixelAddressingXValue = (U32)parms[FEATURE_PIXEL_ADDRESSING_PARAM_VALUE];
            pixelAddressingYValue = (U32)parms[FEATURE_PIXEL_ADDRESSING_PARAM_VALUE];
        } else {
            pixelAddressingXValue = (U32)parms[FEATURE_PIXEL_ADDRESSING_PARAM_X_VALUE];
            pixelAddressingYValue = (U32)parms[FEATURE_PIXEL_ADDRESSING_PARAM_Y_VALUE];
        }
    }
    // We can calculate the number of pixels now.
    numPixels = (float)((roiWidth / pixelAddressingXValue) * (roiHeight / pixelAddressingYValue));

    return (U32) (numPixels);
}

// returns the size of images from the camera (in bytes)
ULONG  PxLCamera::imageSizeInBytes ()
{
    PXL_RETURN_CODE rc = ApiSuccess;

    float parms[4];
    U32 pixelFormat;
    float numPixels = (float) imageSizeInPixels();
    U32 flags = FEATURE_FLAG_MANUAL;
    U32 numParams;

    assert(0 != m_hCamera);

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
        if (API_SUCCESS(rc) ||
            triggering())   // Don't retry if we are triggering -- let the user know the frame was lost.
        {
            break;
        }
    }

    return rc;
}

PXL_RETURN_CODE PxLCamera::formatRgbImage (void*pFrame, FRAME_DESC* pFrameDesc, ULONG bufferSize, void*pImage)
{
    U32 imageSize = bufferSize;
    return PxLFormatImage (pFrame, pFrameDesc, IMAGE_FORMAT_RAW_RGB24_NON_DIB, pImage, &imageSize);
}

PXL_RETURN_CODE PxLCamera::formatImage (void*pFrame, FRAME_DESC* pFrameDesc, ULONG format, ULONG* bufferSize, void*pImage)
{
    return PxLFormatImage (pFrame, pFrameDesc, format, pImage, bufferSize);
}

PXL_RETURN_CODE PxLCamera::captureImage (const char* fileName, ULONG imageType)
{
	return ApiSuccess;
}

PXL_RETURN_CODE PxLCamera::getEncodedClip (const U32 uNumFrames, const U32 uClipDecimationFactor, const U32 uEncoding,
		                                   LPCSTR fileName, float pbFrameRate, int pbBitRate, ClipTerminationCallback termCallback)
{
    CLIP_ENCODING_INFO clipInfo;

    clipInfo.uStreamEncoding = uEncoding;
    clipInfo.uDecimationFactor = uClipDecimationFactor;
    clipInfo.playbackFrameRate = pbFrameRate;
    clipInfo.playbackBitRate = pbBitRate;

     return PxLGetEncodedClip(m_hCamera,
                              uNumFrames,
                              fileName,
                              &clipInfo,
                              termCallback);
}

PXL_RETURN_CODE PxLCamera::formatH264Clip (LPCSTR encodedFile, LPCSTR videoFile, ULONG videoFormat)
{
    return PxLFormatClipEx (encodedFile, videoFile, CLIP_ENCODING_H264, videoFormat);
}

PXL_RETURN_CODE PxLCamera::setSsUpdateCallback (SharpnessScoreUpdateCallback updateFunc)
{
    m_ssUpdateFunc = updateFunc;
    return PxLSetCallback (m_hCamera, OVERLAY_FRAME, this, updateFunc ? PxLFrameCallback : NULL);
}

PXL_RETURN_CODE PxLCamera::setPreviewCallback (PxLApiCallback callback, LPVOID pContext)
{
    return PxLSetCallback (m_hCamera, OVERLAY_PREVIEW, pContext, callback);
}

PXL_RETURN_CODE PxLCamera::getCameraInfo (CAMERA_INFO &cameraInfo)
{
    return PxLGetCameraInfoEx (m_hCamera, &cameraInfo, sizeof(cameraInfo));
}

PXL_RETURN_CODE PxLCamera::getCameraIdInfo (CAMERA_ID_INFO &cameraIdInfo)
{
    cameraIdInfo = m_cameraIdInfo;
    return ApiSuccess;
}

PXL_RETURN_CODE PxLCamera::loadSettings (bool factoryDefaults)
{
    return PxLLoadSettings (m_hCamera, factoryDefaults ? PXL_SETTINGS_FACTORY : PXL_SETTINGS_USER);
}

PXL_RETURN_CODE PxLCamera::saveSettings ()
{
    return PxLSaveSettings (m_hCamera, PXL_SETTINGS_USER);
}

//  Determine what, if anything, is limiting the camera's choice for actual frame rate.
FRAME_RATE_LIMITER PxLCamera::actualFrameRatelimiter ()
{
    PXL_RETURN_CODE rc;

    //
    // Step 1
    //      If the camera is free to choose the frame rate, then there is no limiter
    bool cameraChoosingFramerate = false;
    rc = getContinuousAuto (FEATURE_FRAME_RATE, &cameraChoosingFramerate);
    if (API_SUCCESS(rc) && cameraChoosingFramerate) return FR_LIMITER_NONE;

    //
    // Step 2
    //      At this point, we know the user is attempting to control the frame rate.
    //      However, the camera might not be able to actually use that frame rate.
    //      We need to see if one of FEATURE_EXPOSURE or FEATURE_BANDWIDTH_LIMIT is
    //      preventing it.  BUT, in order to determine this, we need to support
    //      FEATURE_ACTUAL_FRAME_RATE
    if (! supported(FEATURE_ACTUAL_FRAME_RATE)) return FR_LIMITER_USER;
    float actualFrameRate = 0.0;
    float userSetFrameRate = 0.0;
    rc = getValue(FEATURE_ACTUAL_FRAME_RATE, &actualFrameRate);
    if (!API_SUCCESS(rc)) return FR_LIMITER_USER;
    rc = getValue(FEATURE_FRAME_RATE, &userSetFrameRate);
    if (!API_SUCCESS(rc)) return FR_LIMITER_USER;

    if (actualFrameRate >= userSetFrameRate) return FR_LIMITER_USER;

    //
    // Step 3
    //      Something is limiting the actual frame rate.  Start with figuring out the
    //      bandwidth requirements based solely on exposure
    float exposure = 1.0;
    rc = getValue(FEATURE_EXPOSURE, &exposure);
    if (!API_SUCCESS(rc)) return FR_LIMITER_USER;
    float exposureLimit = (1.0 / exposure) * imageSizeInBytes(); // must be in units of BytePS

    //
    // Step 4
    //      Figure out the bandwidth limit (if supported)
    float bandwidthLimit = exposureLimit;
    float bwLimitValue;
    if (supported(FEATURE_BANDWIDTH_LIMIT) && enabled (FEATURE_BANDWIDTH_LIMIT))
    {
        rc = getValue(FEATURE_BANDWIDTH_LIMIT, &bwLimitValue);
        if (API_SUCCESS(rc))
        {
            bandwidthLimit = bwLimitValue * (1000000.0 / 8.0);   // convert from MbitsPS to BytesPS
        }
    }

    //
    // Step 5
    //      So which is it that is limiting the frame rate
    if (bandwidthLimit < exposureLimit) return FR_LIMITER_BANDWIDTH_LIMIT;

    //
    // Step 6
    //      Determine if we are being limited by HDR mode
    float hdrMode = FEATURE_GAIN_HDR_MODE_NONE;

    if (supported(FEATURE_GAIN_HDR) && enabled (FEATURE_GAIN_HDR))
    {
        getValue(FEATURE_GAIN_HDR, &hdrMode);
    }

    if (hdrMode == FEATURE_GAIN_HDR_MODE_INTERLEAVED)
    {
        return FR_LIMITER_HDR_INTERLEAVED;

    }

    // The one choice left...
    return FR_LIMITER_EXPOSURE;

}

PXL_RETURN_CODE PxLCamera::setEventCallback (int eventId, void* context, EventCallback callback)
{
    return PxLSetEventCallback (m_hCamera, eventId, context, callback);
}


/* ---------------------------------------------------------------------------
 * --   Member functions : private
 * ---------------------------------------------------------------------------
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

static U32 PxLFrameCallback (
        HANDLE hCamera,
        LPVOID pFrameData,
        U32    uDataFormat,
        FRAME_DESC const * pFramedesc,
        LPVOID pContext)
{
    if (pContext)
    {
        PxLCamera* pCamera = (PxLCamera*) pContext;
        if (pCamera->m_ssUpdateFunc && pFramedesc)
        {
            (pCamera->m_ssUpdateFunc) (pFramedesc->SharpnessScore.fValue);
        }
    }

    return ApiSuccess;
}


