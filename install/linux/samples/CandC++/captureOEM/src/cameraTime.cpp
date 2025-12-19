/***************************************************************************
 *
 *     File: time.cpp
 *
 *     Description:
 *        Controls for the 'Time' tab  in CaptureOEM.
 */

#include <stdlib.h>
#include <glib.h>
#include "cameraTime.h"
#include "camera.h"
#include "captureOEM.h"
#include "cameraSelect.h"
#include "helpers.h"
#include "controls.h"
#include "onetime.h"

using namespace std;

extern PxLTime         *gTimeTab;
extern PxLCameraSelect *gCameraSelectTab;
extern PxLControls     *gControlsTab;

//
// Local prototypes.
//    UI updates can only be done from a gtk thread -- these routines are gtk 'idle' threads
//    and as such are 'UI update safe'. For each 'feature', there there the following functions:
//       . {featureXXX}Deactivate - Makes the controls meaningless (including greying them out)
//       . {featreuXXX}Activate - updates the controls with values from the camera
static gboolean  RefreshComplete (gpointer pData);
static gboolean  PtpDeactivate (gpointer pData);
static gboolean  PtpActivate (gpointer pData);

// Prototypes for functions used update the temperature.
PXL_RETURN_CODE PxLGetCurrentTime();
void PxLUpdateTimeDisplay();
const PxLFeaturePollFunctions PxLTimeFuncs (PxLGetCurrentTime, PxLUpdateTimeDisplay);

// Indexed by FEATURE_PTP_STATUS_INITIALIZING from PixeLINKTypes.h
static const char * const PxLPtpStatusStrings[] =
{
   "Disabled",  // There is no value for '0' status -- so this is just a placeholder
   "Initializing",
   "Faulty",
   "Disabled",
   "Listening",
   "Premaster",
   "Master",
   "Passive",
   "Uncalibrated",
   "Slave"
};

/* ---------------------------------------------------------------------------
 * --   Member functions - Public
 * ---------------------------------------------------------------------------
 */
PxLTime::PxLTime (GtkBuilder *builder)
: m_SupportsGetTimestamp(false)
, m_PtpIsEnabled(false)
, m_DaysLast(0)
, m_HoursLast(0)
, m_MinutesLast(0)
, m_SecondsLast(0.0)
, m_PtpStatusLast (FEATURE_PTP_STATUS_DISABLED)
{
    //
    // Step 1
    //      Find all of the glade controls

    m_ptpEnable = GTK_WIDGET( gtk_builder_get_object( builder, "PtpEnable_Checkbutton" ) );
    m_ptpSlaveOnly = GTK_WIDGET( gtk_builder_get_object( builder, "PtpSlaveOnly_Checkbutton" ) );
    m_ptpState = GTK_WIDGET( gtk_builder_get_object( builder, "PtpState_Text" ) );

    m_TimeDays = GTK_WIDGET( gtk_builder_get_object( builder, "TimeDays_Text" ) );
    m_TimeHours = GTK_WIDGET( gtk_builder_get_object( builder, "TimeHours_Text" ) );
    m_TimeMinutes = GTK_WIDGET( gtk_builder_get_object( builder, "TimeMinutes_Text" ) );
    m_TimeSeconds = GTK_WIDGET( gtk_builder_get_object( builder, "TimeSeconds_Text" ) );
}


PxLTime::~PxLTime ()
{
}

void PxLTime::refreshRequired (bool noCamera)
{
    if (IsActiveTab (TimeTab))
    {
        if (noCamera)
        {
            // If I am the active tab, then grey out everything
            gdk_threads_add_idle ((GSourceFunc)PtpDeactivate, this);
            if (gCamera) gCamera->m_poller->pollRemove(PxLTimeFuncs);
        } else {
            // If I am the active tab, then refresh everything
            gdk_threads_add_idle ((GSourceFunc)PtpActivate, this);
            gCamera->m_poller->pollAdd(PxLTimeFuncs);
        }

        gdk_threads_add_idle ((GSourceFunc)RefreshComplete, this);
        m_numRefreshRequestsOutstanding++;
    } else {
        // If we are not the active tab, only bump the m_numRefreshRequestsOutstanding if there is not
        // one outstanding already; RefreshComplete will be scheduled when the tab becomes active
        if (!m_numRefreshRequestsOutstanding)m_numRefreshRequestsOutstanding++;
    }
}

void PxLTime::activate()
{
    // I have become the active tab.

    if (gCamera)
    {
        if (m_numRefreshRequestsOutstanding)
        {
            gdk_threads_add_idle ((GSourceFunc)PtpActivate, this);
        }
        gCamera->m_poller->pollAdd(PxLTimeFuncs);
    } else {
        gdk_threads_add_idle ((GSourceFunc)PtpDeactivate, this);
    }

    m_numRefreshRequestsOutstanding = 1; // As a safety mechanism, tab activation should assert value, it will be set to 0 when RefreshComplete
    gdk_threads_add_idle ((GSourceFunc)RefreshComplete, this);
}

void PxLTime::deactivate()
{
    // I am no longer the active tab.
    if (gCamera)
    {
        gCamera->m_poller->pollRemove(PxLTimeFuncs);
    }
}


/* ---------------------------------------------------------------------------
 * --   gtk thread callbacks - used to update controls
 * ---------------------------------------------------------------------------
 */

// Indicate that the refresh is no longer outstanding, it has completed.
static gboolean RefreshComplete (gpointer pData)
{
    PxLTime *pControls = (PxLTime *)pData;

    pControls->m_numRefreshRequestsOutstanding--;
    return false;
}

// Make PTP controls unselectable
static gboolean PtpDeactivate (gpointer pData)
{
    PxLTime *pControls = (PxLTime *)pData;

    gtk_widget_set_sensitive (pControls->m_ptpEnable, false);
    gtk_widget_set_sensitive (pControls->m_ptpSlaveOnly, false);
    gtk_widget_set_sensitive (pControls->m_ptpState, false);

    //pControls->m_PtpIsEnabled = false;

    return false;  //  Only run once....
}

//
// Make PTP controls selectable (if appropriate)
static gboolean PtpActivate (gpointer pData)
{
    PxLTime *pControls = (PxLTime *)pData;

    bool supported = false;
    bool enabled = false;

    PxLAutoLock lock(&gCameraLock);
    int mode = 0;

    if (gCamera)
    {
        if (gCamera->supported(FEATURE_PTP))
        {
            int status = 0;

            supported = true;
            enabled = gCamera->enabled (FEATURE_PTP);

            if (API_SUCCESS(gCamera->getPtpStatus(&status, &mode)))
            {
                if (status < (int)(sizeof(PxLPtpStatusStrings) / sizeof(&PxLPtpStatusStrings[0])))
                {
                    gtk_entry_set_text (GTK_ENTRY (pControls->m_ptpState), PxLPtpStatusStrings[status]);
                }
            }

        }
    }

    gtk_widget_set_sensitive (pControls->m_ptpEnable, supported);
    gtk_widget_set_sensitive (pControls->m_ptpSlaveOnly, supported);
    gtk_widget_set_sensitive (pControls->m_ptpState, supported & enabled);
    pControls->m_PtpIsEnabled = supported & enabled;
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(gTimeTab->m_ptpEnable), pControls->m_PtpIsEnabled);
    if (pControls->m_PtpIsEnabled)
    {
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(gTimeTab->m_ptpSlaveOnly), mode == FEATURE_PTP_MODE_SLAVE_ONLY);
    }

    return false;  //  Only run once....
}

//
// Called periodically -- reads the current camera time
PXL_RETURN_CODE PxLGetCurrentTime()
{
    PXL_RETURN_CODE rc = ApiSuccess;

    PxLAutoLock lock(&gCameraLock);
    if (gCamera && gTimeTab)
    {
        double timestamp = 0.0;

        rc = gCamera->getCurrentTimestamp(&timestamp);
        if (!API_SUCCESS(rc)) return rc;

        gTimeTab->m_SupportsGetTimestamp = true;

        gTimeTab->m_DaysLast = static_cast<U32>(timestamp / (60.0*60.0*24.0));
        timestamp -= gTimeTab->m_DaysLast * (60.0*60.0*24.0);
        gTimeTab->m_HoursLast = static_cast<U32>(timestamp / (60.0*60.0));
        timestamp -= gTimeTab->m_HoursLast * (60.0*60.0);
        gTimeTab->m_MinutesLast = static_cast<U32>(timestamp / 60.0);
        timestamp -= gTimeTab->m_MinutesLast * 60.0;
        gTimeTab->m_SecondsLast = timestamp;

        int status = 0;
        int mode = 0;
        rc = gCamera->getPtpStatus(&status, &mode);
        if (API_SUCCESS(rc) && status <= (int)(sizeof(PxLPtpStatusStrings) / sizeof(&PxLPtpStatusStrings[0])))
        {
            gTimeTab->m_PtpStatusLast = status;
        }
    }

    return rc;
}

//
// Called periodically -- updates the current time controls
void PxLUpdateTimeDisplay()
{
    if (gCamera && gTimeTab)
    {
        if (gTimeTab->m_SupportsGetTimestamp)
        {
            char cActualValue[40];

            sprintf (cActualValue, "%d", gTimeTab->m_DaysLast);
            gtk_entry_set_text (GTK_ENTRY (gTimeTab->m_TimeDays), cActualValue);
            sprintf (cActualValue, "%d", gTimeTab->m_HoursLast);
            gtk_entry_set_text (GTK_ENTRY (gTimeTab->m_TimeHours), cActualValue);
            sprintf (cActualValue, "%d", gTimeTab->m_MinutesLast);
            gtk_entry_set_text (GTK_ENTRY (gTimeTab->m_TimeMinutes), cActualValue);
            sprintf (cActualValue, "%5.2f", gTimeTab->m_SecondsLast);
            gtk_entry_set_text (GTK_ENTRY (gTimeTab->m_TimeSeconds), cActualValue);
        }

        if (gTimeTab->m_PtpIsEnabled)
        {
            gtk_entry_set_text (GTK_ENTRY (gTimeTab->m_ptpState), PxLPtpStatusStrings[gTimeTab->m_PtpStatusLast]);
        }
    }
}

/* ---------------------------------------------------------------------------
 * --   Control functions from the Glade project
 * ---------------------------------------------------------------------------
 */

extern "C" void PtpEnableToggled
    (GtkWidget* widget, GdkEventExpose* event, gpointer userdata )
{
    if (! gCamera || !gTimeTab) return;
    if (gTimeTab->m_numRefreshRequestsOutstanding) return;

    bool enable = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON(gTimeTab->m_ptpEnable));
    bool slaveOnly = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON(gTimeTab->m_ptpSlaveOnly));

    PxLAutoLock lock(&gCameraLock);

    if (enable)
    {
        gCamera->setPtpMode (slaveOnly ? FEATURE_PTP_MODE_SLAVE_ONLY : FEATURE_PTP_MODE_AUTOMATIC);
    } else {
        gCamera->setPtpMode (FEATURE_PTP_MODE_DISABLED);
    }
}

extern "C" void PtpSlaveOnlyToggled
    (GtkWidget* widget, GdkEventExpose* event, gpointer userdata )
{
    // This will also do an enable/disable of PTP
    PtpEnableToggled (widget, event, userdata);
}

