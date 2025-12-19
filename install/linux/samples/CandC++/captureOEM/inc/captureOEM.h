
/***************************************************************************
 *
 *     File: C-OEMLite.h
 *
 *     Description: Top level header file for the application.
 *
 *     Notes:  See design notes at at the top of simpleGui.cpp
 *
 */

#if !defined(PIXELINK_CAPTURE_OEM_H)
#define PIXELINK_CAPTURE_OEM_H

#include <gtk/gtk.h>
#include <pthread.h>
#include "tab.h"

// Bugzilla.2913
//   Remove video capture for licensing reasons
//#define VIDEO_CAPTURE_SUPPORTED 1


// Our app uses OneTime in favor of OnePush
#define FEATURE_FLAG_ONETIME FEATURE_FLAG_ONEPUSH

typedef enum _C_OEM_TABS
{
    ControlsTab,
    FirstTab = ControlsTab,
    StreamTab,
    ImageTab,
#if defined VIDEO_CAPTURE_SUPPORTED
    VideoTab,
#endif
    GpioTab,
    LensTab,
    FilterTab,
    LinkTab,
    AutoRoiTab,
    TimeTab,
    InfoTab,
    LastTab = InfoTab
} C_OEM_TABS;

bool IsActiveTab (C_OEM_TABS tab);
PxLTab* GetActiveTab ();
void GrabCamera (PCAMERA_ID_INFO pNewCameraIdInfo);
void ReleaseCamera ();

//
// Mutex control to allow multiple threads access to critical resources
class PxLAutoLock
{
public:
    // Note that we are using pthread mutexes, as opposed to gthread.  pthread mutexes can be
    // used in a nested fashion, gthread mutexes cannot.
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
extern PxLCamera       *gCamera;
extern pthread_mutex_t  gCameraLock;

extern char CaptureOEMVersion[];

#endif // !defined(PIXELINK_CAPTURE_OEM_H)
