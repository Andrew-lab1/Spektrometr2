
/***************************************************************************
 *
 *     File: camera.h
 *
 *     Description: Defines the camera class used by the simple GUI application
 *
 */

#if !defined(PIXELINK_CAMERA_H)
#define PIXELINK_CAMERA_H

#include <gtk/gtk.h>
#include <stdio.h>
#include <assert.h>
#include <memory>
#include "PixeLINKApi.h"
#include "featurePoller.h"
#include "roi.h"
#include "pixelFormat.h"

//
// A very simple PixeLINKApi exception handler
//
class PxLError
{
public:
	PxLError(PXL_RETURN_CODE rc):m_rc(rc){};
	~PxLError(){};
	char *showReason()
	{
		sprintf (m_msg, "PixeLINK API returned an error of 0x%08X", m_rc);
		return (m_msg);
	}
private:
	char m_msg[256];  // Large enough for all of our messages
public:
	PXL_RETURN_CODE m_rc;
};

extern "C" typedef U32 (* ClipTerminationCallback)(HANDLE, U32, PXL_RETURN_CODE);

extern "C" typedef void (* SharpnessScoreUpdateCallback)(float);

extern "C" typedef U32 ( * PxLApiCallback)(HANDLE, LPVOID, U32, FRAME_DESC const *, LPVOID);

extern "C" typedef U32 ( * EventCallback)(HANDLE, U32, double, U32, LPVOID, LPVOID);

// How is the camera determining the actual frame rate?
typedef enum _FRAME_RATE_LIMITER
{
    FR_LIMITER_NONE,   // The camera is free to choose the optimal frame rate given all of the other settings
    FR_LIMITER_USER,   // The user is controlling the frame rate, not the camera.  That value is limiting the frame rate
    FR_LIMITER_EXPOSURE, // The user defined exposure is what is limiting the frame rate
    FR_LIMITER_BANDWIDTH_LIMIT, // The bandwidth limit is what is dictating the frame rate
    FR_LIMITER_HDR_INTERLEAVED, // The camera cannot achieve desired frame rate because it is in HDR Interleaved mode
} FRAME_RATE_LIMITER;

class PxLTriggerInfo
{
public:
    bool m_enabled;

    PxLTriggerInfo();

    float m_mode;
    float m_type;
    float m_polarity;
    float m_delay;
    float m_number;
};

class PxLGpioInfo
{
public:
    bool m_enabled;

    PxLGpioInfo();

    float m_mode;
    float m_polarity;
    // The meaning of the last 3 parameters, vary with the mode
    float m_param1;
    float m_param2;
    float m_param3;
};

class PxLCamera
{
    friend class PxLInterruptStream;
public:

    // Constructor
    PxLCamera (PCAMERA_ID_INFO pCameraIdInfo);
    // Destructor
    ~PxLCamera();

    ULONG serialNum();
    HANDLE getHandle();

    // assert the preview/stream state
    PXL_RETURN_CODE play();
    PXL_RETURN_CODE pause();
    PXL_RETURN_CODE suspend();
    PXL_RETURN_CODE stop();

    PXL_RETURN_CODE resizePreviewToRoi();
    PXL_RETURN_CODE pausePreview();
    PXL_RETURN_CODE playPreview();
    PXL_RETURN_CODE stopStream();
    PXL_RETURN_CODE pauseStream();
    PXL_RETURN_CODE startStream();

    //generic feature services
    bool supported (ULONG feature);
    bool supportedInCamera (ULONG feature);
    bool enabled (ULONG feature);
    bool manualSupported (ULONG feature);
    bool oneTimeSuppored (ULONG feature);
    bool continuousSupported (ULONG feature);
    bool settable (ULONG feature);
    ULONG numParametersSupported (ULONG feature);
    bool streaming();
    bool streamStopped();
    bool previewing();
    bool triggering();
    bool hardwareTriggering();
    PXL_RETURN_CODE disable (ULONG feature);

    // more specific feature services for simple features (features with just a single
    // 'float' parameter)
    PXL_RETURN_CODE getRange (ULONG feature, float* min, float* max);
    PXL_RETURN_CODE getValue (ULONG feature, float* value);
    PXL_RETURN_CODE get2ndValue (ULONG feature, float firstValue, float* secondValue);
    PXL_RETURN_CODE setValue (ULONG feature, float value);
    PXL_RETURN_CODE set2ndValue (ULONG feature, float firstValue, float secondValue);
    PXL_RETURN_CODE getOnetimeAuto (ULONG feature, bool *stillRunning); // sets true if camera is performing onetime auto
    PXL_RETURN_CODE setOnetimeAuto (ULONG feature, bool enable);
    PXL_RETURN_CODE setOnetimeAuto (ULONG feature, bool enable, float min, float max);
    PXL_RETURN_CODE getContinuousAuto (ULONG feature, bool *enable); // sets true if camera is performing continuous auto
    PXL_RETURN_CODE setContinuousAuto (ULONG feature, bool enable);
    PXL_RETURN_CODE setContinuousAuto (ULONG feature, bool enable, float min, float max);
    PXL_RETURN_CODE getAutoLimits (ULONG feature, float* min, float* max);
    // feature services for more complex features (more than one parameter
    PXL_RETURN_CODE getPixelAddressRange (float* minMode, float* maxMode, float* minValue, float* maxValue, bool* asymmetric);
    PXL_RETURN_CODE getPixelAddressValues (float* mode, float* valueX, float* valueY);
    PXL_RETURN_CODE setPixelAddressValues (float mode, float valueX, float valueY);
    PXL_RETURN_CODE getRoiRange (ROI_TYPE type, PXL_ROI* minRoi, PXL_ROI* maxRoi, bool ignoreTransforms = false, float* maxSs = NULL);
    PXL_RETURN_CODE getRoiValue (ROI_TYPE type, PXL_ROI* roi, bool ignoreRotate = false);
    PXL_RETURN_CODE setRoiValue (ROI_TYPE type, PXL_ROI &roi, bool off = false);
    PXL_RETURN_CODE getTriggerRange (float* minMode, float* maxMode, float* minType, float* maxType);
    PXL_RETURN_CODE getTriggerValue (PxLTriggerInfo& trig);
    PXL_RETURN_CODE setTriggerValue (PxLTriggerInfo& trig);
    PXL_RETURN_CODE sendActionCommand (U32 action, double actionDelay);
    PXL_RETURN_CODE getGpioRange (int* numGpios, float* minMode, float* maxMode);
    PXL_RETURN_CODE getGpioValue (int gpioNum, PxLGpioInfo& info);
    PXL_RETURN_CODE setGpioValue (int gpioNum, PxLGpioInfo& info);
    PXL_RETURN_CODE getWhiteBalanceValues (float* red, float* green, float* blue);
    PXL_RETURN_CODE setWhiteBalanceValues (float red, float green, float blue);
    PXL_RETURN_CODE getFlip (bool* horizontal, bool* vertical);
    PXL_RETURN_CODE setFlip (bool horizontal, bool vertical);
    PXL_RETURN_CODE getPtpStatus (int* status, int* mode);
    PXL_RETURN_CODE setPtpMode (int value);
    PXL_RETURN_CODE getCurrentTimestamp (double* value);
    PXL_RETURN_CODE getAlpha (int* alphaPixelFormat, ULONG* alphaValue);
    PXL_RETURN_CODE setAlpha (int  alphaPixelFormat, ULONG  alphaValue);

    ULONG  imageSizeInBytes ();
    ULONG  imageSizeInPixels ();
    PXL_RETURN_CODE getNextFrame (ULONG bufferSize, void*pFrame, FRAME_DESC* pFrameDesc);
    PXL_RETURN_CODE formatRgbImage (void*pFrame, FRAME_DESC* pFrameDesc, ULONG bufferSize, void*pImage);
    PXL_RETURN_CODE formatImage (void*pFrame, FRAME_DESC* pFrameDesc, ULONG format, ULONG* bufferSize, void*pImage);
    PXL_RETURN_CODE captureImage (const char* fileName, ULONG imageType);
    PXL_RETURN_CODE getEncodedClip (const U32 uNumFrames, const U32 uClipDecimationFactor, const U32 uEncoding,
    		                        LPCSTR fileName, float pbFrameRate, int pbBitRate,
									ClipTerminationCallback termCallback);
    PXL_RETURN_CODE formatH264Clip (LPCSTR encodedFile, LPCSTR videoFile, ULONG videoFormat);
    PXL_RETURN_CODE setSsUpdateCallback (SharpnessScoreUpdateCallback updateFunc);
    PXL_RETURN_CODE setPreviewCallback (PxLApiCallback callback, LPVOID pContext);
    PXL_RETURN_CODE getCameraInfo (CAMERA_INFO &cameraInfo);
    PXL_RETURN_CODE getCameraIdInfo (CAMERA_ID_INFO &cameraIdInfo);
    PXL_RETURN_CODE loadSettings (bool factoryDefaults);
    PXL_RETURN_CODE saveSettings ();
    FRAME_RATE_LIMITER actualFrameRatelimiter ();

    PXL_RETURN_CODE setEventCallback (int eventId, void* context, EventCallback callback);

    PxLFeaturePoller* m_poller;

    SharpnessScoreUpdateCallback m_ssUpdateFunc;

private:
    PXL_RETURN_CODE getFlags (ULONG feature, ULONG *flags);
    bool   requiresStreamStop (ULONG feature);
    float  pixelSize (ULONG pixelFormat);

    ULONG  m_serialNum; // serial number of our camera
    CAMERA_ID_INFO m_cameraIdInfo; // Information about the camera

    HANDLE m_hCamera;   // handle to our camera

    ULONG  m_streamState;
    ULONG  m_previewState;

    HWND   m_previewHandle;

    COEM_PIXEL_FORMAT_INTERPRETATIONS m_pixelFormatInterpretation;

};

inline ULONG PxLCamera::serialNum()
{
	return m_serialNum;
}

inline HANDLE PxLCamera::getHandle()
{
    return m_hCamera;
}

inline bool PxLCamera::streaming()
{
    return (START_STREAM == m_streamState);
}

inline bool PxLCamera::streamStopped()
{
    return (STOP_STREAM == m_streamState);
}

inline bool PxLCamera::previewing()
{
    return (START_PREVIEW == m_previewState);
}

inline float PxLCamera::pixelSize(ULONG pixelFormat)
{
    ULONG retVal = 0;
    switch(pixelFormat) {

        case PIXEL_FORMAT_MONO8:
        case PIXEL_FORMAT_BAYER8_GRBG:
        case PIXEL_FORMAT_BAYER8_RGGB:
        case PIXEL_FORMAT_BAYER8_GBRG:
        case PIXEL_FORMAT_BAYER8_BGGR:
            retVal = 1.0;
            break;

        case PIXEL_FORMAT_MONO10_PACKED_MSFIRST:
        case PIXEL_FORMAT_BAYER10_GRBG_PACKED_MSFIRST:
        case PIXEL_FORMAT_BAYER10_RGGB_PACKED_MSFIRST:
        case PIXEL_FORMAT_BAYER10_GBRG_PACKED_MSFIRST:
        case PIXEL_FORMAT_BAYER10_BGGR_PACKED_MSFIRST:
            return 1.25f;

        case PIXEL_FORMAT_MONO12_PACKED:
        case PIXEL_FORMAT_BAYER12_GRBG_PACKED:
        case PIXEL_FORMAT_BAYER12_RGGB_PACKED:
        case PIXEL_FORMAT_BAYER12_GBRG_PACKED:
        case PIXEL_FORMAT_BAYER12_BGGR_PACKED:
        case PIXEL_FORMAT_MONO12_PACKED_MSFIRST:
        case PIXEL_FORMAT_BAYER12_GRBG_PACKED_MSFIRST:
        case PIXEL_FORMAT_BAYER12_RGGB_PACKED_MSFIRST:
        case PIXEL_FORMAT_BAYER12_GBRG_PACKED_MSFIRST:
        case PIXEL_FORMAT_BAYER12_BGGR_PACKED_MSFIRST:
            return 1.5f;

        case PIXEL_FORMAT_YUV422:
        case PIXEL_FORMAT_MONO16:
        case PIXEL_FORMAT_BAYER16_GRBG:
        case PIXEL_FORMAT_BAYER16_RGGB:
        case PIXEL_FORMAT_BAYER16_GBRG:
        case PIXEL_FORMAT_BAYER16_BGGR:
            retVal = 2.0;
            break;

        case PIXEL_FORMAT_RGB24:
        case PIXEL_FORMAT_RGB24_NON_DIB:
        case PIXEL_FORMAT_BGR24_NON_DIB:
            retVal = 3.0;
            break;

        case PIXEL_FORMAT_RGBA:
        case PIXEL_FORMAT_BGRA:
        case PIXEL_FORMAT_ARGB:
        case PIXEL_FORMAT_ABGR:
            retVal = 4.0;

        case PIXEL_FORMAT_RGB48:
            retVal = 6.0;
            break;

        case PIXEL_FORMAT_STOKES4_12:
        case PIXEL_FORMAT_POLAR4_12:
        case PIXEL_FORMAT_POLAR_RAW4_12:
        case PIXEL_FORMAT_HSV4_12:
            retVal = 6.0;
            break;

        default:
            assert(0);
            break;
    }
    return retVal;
}

// Declare one of these on the stack to temporarily change the state
// of the video stream within the scope of a code block.  Note that this
// class will also pause the preview (if necessary) -- doing this makes
// the stream interruption a little smoother
class PxLInterruptStream
{
public:
    PxLInterruptStream(PxLCamera* pCam, ULONG newState)
    : m_pCam(pCam)
    {
        m_oldStreamState = pCam->m_streamState;
        m_oldPreviewState = pCam->m_previewState;
        if (newState != m_oldStreamState)
        {
            if (pCam->m_previewState == START_PREVIEW) pCam->pausePreview();
            switch (newState)
            {
            case START_STREAM:
                pCam->startStream();
                break;
            case PAUSE_STREAM:
                pCam->pauseStream();
                break;
            case STOP_STREAM:
            default:
                pCam->stopStream();
            }
        }
    }
    ~PxLInterruptStream()
    {
        if (m_pCam->m_streamState != m_oldStreamState)
        {
            switch (m_oldStreamState)
            {
            case START_STREAM:
                m_pCam->startStream();
                break;
            case PAUSE_STREAM:
                m_pCam->pauseStream();
                break;
            case STOP_STREAM:
            default:
                m_pCam->stopStream();
            }
            if (m_oldPreviewState == START_PREVIEW) m_pCam->playPreview();
        }
    }
private:
    PxLCamera*   m_pCam;
    U32          m_oldStreamState;
    U32          m_oldPreviewState;
};

// define a macro that will conveniently interrupt the stream to make some changes
#define TEMP_STREAM_STOP()                                                                    \
    std::auto_ptr<PxLInterruptStream> _temp_ss(NULL);                                         \
    _temp_ss = std::auto_ptr<PxLInterruptStream>(new PxLInterruptStream(gCamera, STOP_STREAM));  \

// define a macro that will conveniently start the stream to perfrom an operation
#define TEMP_STREAM_START()                                                                    \
    std::auto_ptr<PxLInterruptStream> _temp_ss(NULL);                                         \
    _temp_ss = std::auto_ptr<PxLInterruptStream>(new PxLInterruptStream(gCamera, START_STREAM));  \


#endif // !defined(PIXELINK_CAMERA_H)
