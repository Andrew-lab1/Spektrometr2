
/***************************************************************************
 *
 *     File: camera.h
 *
 *     Description: Defines the camera class used by the simple GUI application
 *
 */

#if !defined(PIXELINK_CAMERA_H)
#define PIXELINK_CAMERA_H

#include "PixeLINKApi.h"
#include "featurePoller.h"
#include "roi.h"
#include <stdio.h>
#include <assert.h>

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

class PxLCamera
{
    friend class PxLInterruptStream;
public:
    // Constructor
    PxLCamera (ULONG serialNum);
    // Destructor
    ~PxLCamera();

    ULONG serialNum();

    // assert the preview/stream state
    PXL_RETURN_CODE play();
    PXL_RETURN_CODE pause();
    PXL_RETURN_CODE stop();

    PXL_RETURN_CODE resizePreviewToRoi();

    //generic feature services
    bool supported (ULONG feature);
    bool oneTimeSuppored (ULONG feature);
    bool continuousSupported (ULONG feature);
    bool streaming();

    // more specific feature services for simple features (features with just a single
    // 'float' parameter)
    PXL_RETURN_CODE getRange (ULONG feature, float* min, float* max);
    PXL_RETURN_CODE getValue (ULONG feature, float* value);
    PXL_RETURN_CODE setValue (ULONG feature, float value);
    PXL_RETURN_CODE performOneTimeAuto (ULONG feature);
    PXL_RETURN_CODE getContinuousAuto (ULONG feature, bool *enable); // sets true if camera is performing continuous auto
    PXL_RETURN_CODE setContinuousAuto (ULONG feature, bool enable);
    // feature services for more complex features (more than one parameter
    PXL_RETURN_CODE getPixelAddressRange (float* minMode, float* maxMode, float* minValue, float* maxValue);
    PXL_RETURN_CODE getPixelAddressValue (float* mode, float* value);
    PXL_RETURN_CODE setPixelAddressValue (float mode, float value);
    PXL_RETURN_CODE getRoiRange (PXL_ROI* minRoi, PXL_ROI* maxRoi);
    PXL_RETURN_CODE getRoiValue (PXL_ROI* roi);
    PXL_RETURN_CODE setRoiValue (PXL_ROI &roi);
    PXL_RETURN_CODE getWhiteBalanceRange (float* min, float* max);
    PXL_RETURN_CODE getWhiteBalanceValues (float* red, float* green, float* blue);
    PXL_RETURN_CODE setWhiteBalanceValues (float red, float green, float blue);
    PXL_RETURN_CODE getFlip (bool* horizontal, bool* vertical);
    PXL_RETURN_CODE setFlip (bool horizontal, bool vertical);

    PXL_RETURN_CODE captureImage (const char* fileName, ULONG imageType);

    PxLFeaturePoller* m_poller;

private:
    PXL_RETURN_CODE getFlags (ULONG feature, ULONG *flags);
    bool   requiresStreamStop (ULONG feature);
    ULONG  imageSize ();
    PXL_RETURN_CODE getNextFrame (ULONG bufferSize, void*pFrame, FRAME_DESC* pFrameDesc);
    float  pixelSize (ULONG pixelFormat);

    ULONG  m_serialNum; // serial number of our camera

    HANDLE m_hCamera;   // handle to our camera

    ULONG  m_streamState;
    ULONG  m_previewState;

    HWND   m_previewHandle;



};

inline ULONG PxLCamera::serialNum()
{
	return m_serialNum;
}

inline bool PxLCamera::streaming()
{
    return (START_STREAM == m_streamState);
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
            retVal = 3.0;
            break;

        case PIXEL_FORMAT_RGB48:
            retVal = 6.0;
            break;

        default:
            assert(0);
            break;
    }
    return retVal;
}

// Declare one of these on the stack to temporarily change the state
// of the video stream within the scope of a code block.
class PxLInterruptStream
{
public:
    PxLInterruptStream(PxLCamera* pCam, ULONG newState)
    : m_pCam(pCam)
    {
        m_oldState = pCam->m_streamState;
        if (newState != m_oldState)
        {
            switch (newState)
            {
            case START_STREAM:
                pCam->play();
                break;
            case PAUSE_STREAM:
                pCam->pause();
                break;
            case STOP_STREAM:
            default:
                pCam->stop();
            }
        }
    }
    ~PxLInterruptStream()
    {
        if (m_pCam->m_streamState != m_oldState)
        {
            switch (m_oldState)
            {
            case START_STREAM:
                m_pCam->play();
                break;
            case PAUSE_STREAM:
                m_pCam->pause();
                break;
            case STOP_STREAM:
            default:
                m_pCam->stop();
            }
        }
    }
private:
    PxLCamera*   m_pCam;
    U32          m_oldState;
};

#endif // !defined(PIXELINK_CAMERA_H)
