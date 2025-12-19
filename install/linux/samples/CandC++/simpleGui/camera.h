
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
#include <stdio.h>

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

private:

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

#endif // !defined(PIXELINK_CAMERA_H)
