/***************************************************************************
 *
 *     File: CameraTime.h
 *
 *     Description:
 *         Controls for the 'Time' tab  in CaptureOEM.
 *
 */

#if !defined(PIXELINK_CAMERATIME_H)
#define PIXELINK_CAMERATIME_H

#include <gtk/gtk.h>
#include <PixeLINKApi.h>
#include "tab.h"

class PxLTime : public PxLTab
{
public:

    // Constructor
    PxLTime (GtkBuilder *builder);
    // Destructor
    ~PxLTime ();

    void activate ();   // the user has selected this tab
    void deactivate (); // the user has un-selected this tab
    void refreshRequired (bool noCamera);  // Camera status has changed, requiring a refresh of controls

    //
    // All of the controls

    GtkWidget    *m_ptpEnable;
    GtkWidget    *m_ptpSlaveOnly;
    GtkWidget    *m_ptpState;
    GtkWidget    *m_TimeDays;
    GtkWidget    *m_TimeHours;
    GtkWidget    *m_TimeMinutes;
    GtkWidget    *m_TimeSeconds;

    bool          m_SupportsGetTimestamp;
    bool          m_PtpIsEnabled;

    //Last read time
    U32           m_DaysLast;
    U32           m_HoursLast;
    U32           m_MinutesLast;
    double        m_SecondsLast;

    int           m_PtpStatusLast;  // last read PtpStatus;
};

#endif // !defined(PIXELINK_CAMERATIME_H)
