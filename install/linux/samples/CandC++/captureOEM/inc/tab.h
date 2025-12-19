/***************************************************************************
 *
 *     File: tab.h
 *
 *     Description:
 *         Common object for all tabs  in CaptureOEM.
 *
 */

#if !defined(PIXELINK_TAB_H)
#define PIXELINK_TAB_H

#include "PixeLINKApi.h"

#define REFRESH_NONE        0x00
#define REFRESH_NO_CAMERA   0x01
#define REFRESH_NEW_CAMERA  0x02
#define REFRESH_STALE_DATA  0x04

class PxLTab
{
public:

    // COnstructor Destructor
    PxLTab () : m_numRefreshRequestsOutstanding (0) {}
    virtual ~PxLTab () {return;}

    virtual void activate () {return;}   // the user has selected this tab
    virtual void deactivate () {return;} // the user has un-selected this tab
    virtual void refreshRequired (bool noCamera) {return;}  // Camera status has changed, requiring a refresh of controls

    // Bugzilla.1859
    // m_numRefreshRequestsOutstanding serves 2 purposes
    //    1. As an indication that some sort of change happened that requires us to refresh of the controls
    //    2. As an indication that the controls are being updated not because the user changed it's value, but because
    //       something else has changed.
    //    A count greater than 0 indicates that the application is still updating the controls to the current camera values
    //    Really the count should mean the following:
    //       0 - the tab is up too date
    //       1 - Typically a tab whose controls are stale, and will be refreshed the next time it is activated
    //       2 - TYpically only occurs if the current camera is being swapped for a new one; the intermediate value
    //           of 1 is because the tab transfers to the 'no camera' as it is uninitialized
    ULONG m_numRefreshRequestsOutstanding;

};

#endif // !defined(PIXELINK_TAB_H)
