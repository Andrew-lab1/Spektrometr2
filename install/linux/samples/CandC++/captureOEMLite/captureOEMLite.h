
/***************************************************************************
 *
 *     File: C-OEMLite.h
 *
 *     Description: Top level header file for the application.
 *
 *     Notes:  See design notes at at the top of simpleGui.cpp
 *
 */

#if !defined(PIXELINK_SIMPLE_GUI_H)
#define PIXELINK_SIMPLE_GUI_H

#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <pthread.h>
#include "camera.h"
#include "featurePoller.h"
#include "cameraSelectCombo.h"
#include "previewButtons.h"
#include "exposure.h"
#include "pixelFormat.h"
#include "pixelAddress.h"
#include "roi.h"
#include "frameRate.h"
#include "gain.h"
#include "focus.h"
#include "whiteBalance.h"
#include "imageFlip.h"
#include "imageCapture.h"


// Our app uses OneTime in favor of OnePush
#define FEATURE_FLAG_ONETIME FEATURE_FLAG_ONEPUSH

class PxLAutoLock
{
public:
    // Note that we are using pthread mutexes, as opposed to gthread.  pthread mutexes cam be
    // used in a nested fashio, gthread mutexes cannot.
    explicit PxLAutoLock(pthread_mutex_t *mutex, bool lock=true)
    : m_mutex(mutex)
    , m_locked(lock)
    {
        if (lock) pthread_mutex_lock (mutex);
    }
    ~PxLAutoLock()
    {
        if (m_locked) pthread_mutex_unlock (m_mutex);
    }
private:
    pthread_mutex_t *m_mutex;
    bool             m_locked;
};

// The currently selected camera.  A NULL value indicates no camera has been selected.  Note that
//       This 'global' is accessed from multiple threads.  In particular, the active camera can be
//       removed and redefined by the camera scanThread.  We will use a mutex to proect ourselves
//       from issues that could otherwise happen.  Users of pCamera, should grab the mutex first.  The
//       class PxLAutoLock is a convenient way to do this.
extern PxLCamera       *pCamera;
extern pthread_mutex_t  pCameraLock;

// Our GUI control objects.
extern PxLCameraSelectCombo *csObject;
extern PxLPreviewButtons *previewObject;
extern PxLExposure *exposureObject;
extern PxLPixelFormat *pixelFormatObject;
extern PxLPixelAddress *pixelAddressObject;
extern PxLRoi *roiObject;
extern PxLFrameRate *frameRateObject;
extern PxLGain *gainObject;
extern PxLFocus *focusObject;
extern PxLWhiteBalance *whiteBalanceObject;
extern PxLImageFlip *imageFlipObject;
extern PxLImageCapture *imageCaptureObject;

#endif // !defined(PIXELINK_SIMPLE_GUI_H)
