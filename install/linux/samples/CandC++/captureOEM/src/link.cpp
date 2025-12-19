/***************************************************************************
 *
 *     File: link.cpp
 *
 *     Description:
 *        Controls for the 'Link' tab  in CaptureOEM.
 */

#include <stdlib.h>
#include <glib.h>
#include "link.h"
#include "camera.h"
#include "captureOEM.h"
#include "cameraSelect.h"
#include "helpers.h"
#include "controls.h"
#include "onetime.h"

using namespace std;

extern PxLLink         *gLinkTab;
extern PxLCameraSelect *gCameraSelectTab;
extern PxLControls     *gControlsTab;
extern PxLOnetime      *gOnetimeDialog;

//
// Local prototypes.
//    UI updates can only be done from a gtk thread -- these routines are gtk 'idle' threads
//    and as such are 'UI update safe'. For each 'feature', there there the following functions:
//       . {featureXXX}Deactivate - Makes the controls meaningless (including greying them out)
//       . {featreuXXX}Activate - updates the controls with values from the camera
static gboolean  RefreshComplete (gpointer pData);
static gboolean  MaxPacketSizeDeactivate (gpointer pData);
static gboolean  MaxPacketSizeActivate (gpointer pData);
static gboolean  BwLimitDeactivate (gpointer pData);
static gboolean  BwLimitActivate (gpointer pData);

// Prototypes for functions used for features with auto modes (continuous and onetime).
static PXL_RETURN_CODE GetCurrentMaxPacketSize();
static void UpdateMaxPacketSizeControls();
static const PxLFeaturePollFunctions maxPacketSizeFuncs (GetCurrentMaxPacketSize, UpdateMaxPacketSizeControls);

/* ---------------------------------------------------------------------------
 * --   Member functions - Public
 * ---------------------------------------------------------------------------
 */
PxLLink::PxLLink (GtkBuilder *builder)
: m_maxPacketSizeLast (0)
{
    //
    // Step 1
    //      Find all of the glade controls

    m_maxPacketSize = GTK_WIDGET( gtk_builder_get_object( builder, "MaxPacketSize_Text" ) );
    m_maxPacketSizeOneTime = GTK_WIDGET( gtk_builder_get_object( builder, "MaxPacketSizeAuto_Button" ) );

    m_bwLimitLabel = GTK_WIDGET( gtk_builder_get_object( builder, "BandwidthLimit_Label" ) );
    m_bwLimitEnable = GTK_WIDGET( gtk_builder_get_object( builder, "BandwidthLimitEnable_Checkbutton" ) );

    m_bwLimitSlider = new PxLSlider (
            GTK_WIDGET( gtk_builder_get_object( builder, "BandwidthLimitMin_Label" ) ),
            GTK_WIDGET( gtk_builder_get_object( builder, "BandwidthLimitMax_Label" ) ),
            GTK_WIDGET( gtk_builder_get_object( builder, "BandwidthLimit_Scale" ) ),
            GTK_WIDGET( gtk_builder_get_object( builder, "BandwidthLimit_Text" ) ));
}


PxLLink::~PxLLink ()
{
}

void PxLLink::refreshRequired (bool noCamera)
{
    if (IsActiveTab (LinkTab))
    {
        if (noCamera)
        {
            // If I am the active tab, then grey out everything
            gdk_threads_add_idle ((GSourceFunc)MaxPacketSizeDeactivate, this);
            gdk_threads_add_idle ((GSourceFunc)BwLimitDeactivate, this);
        } else {
            // If I am the active tab, then refresh everything
            gdk_threads_add_idle ((GSourceFunc)MaxPacketSizeActivate, this);
            gdk_threads_add_idle ((GSourceFunc)BwLimitActivate, this);
        }

        gdk_threads_add_idle ((GSourceFunc)RefreshComplete, this);
        m_numRefreshRequestsOutstanding++;
    } else {
        // If we are not the active tab, only bump the m_numRefreshRequestsOutstanding if there is not
        // one outstanding already; RefreshComplete will be scheduled when the tab becomes active
        if (!m_numRefreshRequestsOutstanding)m_numRefreshRequestsOutstanding++;
    }
}

void PxLLink::activate()
{
    // I have become the active tab.

    if (gCamera)
    {
        if (m_numRefreshRequestsOutstanding)
        {
            gdk_threads_add_idle ((GSourceFunc)MaxPacketSizeActivate, this);
            gdk_threads_add_idle ((GSourceFunc)BwLimitActivate, this);
        }
    } else {
        gdk_threads_add_idle ((GSourceFunc)MaxPacketSizeDeactivate, this);
        gdk_threads_add_idle ((GSourceFunc)BwLimitDeactivate, this);
    }

    m_numRefreshRequestsOutstanding = 1; // As a safety mechanism, tab activation should assert value, it will be set to 0 when RefreshComplete
    gdk_threads_add_idle ((GSourceFunc)RefreshComplete, this);
}

void PxLLink::deactivate()
{
    // I am no longer the active tab.
}


// indication that the app has transitioned to/from streaming state.
void PxLLink::streamChange (bool streaming)
{
    // The max packet size controls need to enable and disable with the stream
    MaxPacketSizeActivate (gLinkTab);
}

/* ---------------------------------------------------------------------------
 * --   gtk thread callbacks - used to update controls
 * ---------------------------------------------------------------------------
 */

// Indicate that the refresh is no longer outstanding, it has completed.
static gboolean RefreshComplete (gpointer pData)
{
    PxLLink *pLink = (PxLLink *)pData;

    pLink->m_numRefreshRequestsOutstanding--;
    return false;
}

// Make Max Packet Size controls unselectable
static gboolean MaxPacketSizeDeactivate (gpointer pData)
{
    PxLLink *pLink = (PxLLink *)pData;

    gtk_widget_set_sensitive (pLink->m_maxPacketSize, false);
    gtk_widget_set_sensitive (pLink->m_maxPacketSizeOneTime, false);

    return false;  //  Only run once....
}

//
// Make Max Packet size controls selectable (if appropriate)
static gboolean MaxPacketSizeActivate (gpointer pData)
{
    PxLLink *pLink = (PxLLink *)pData;

    bool supported = false;
    bool enabled = false;
    bool streaming = false;

    PxLAutoLock lock(&gCameraLock);

    if (gCamera)
    {
        if (gCamera->supported(FEATURE_MAX_PACKET_SIZE))
        {
            float value;

            supported = true;
            enabled = gCamera->enabled (FEATURE_MAX_PACKET_SIZE);

            if (API_SUCCESS(gCamera->getValue(FEATURE_MAX_PACKET_SIZE, &value)))
            {
                char cValue[40];
                sprintf (cValue, "%d",(U32)value);
                gtk_entry_set_text (GTK_ENTRY (pLink->m_maxPacketSize), cValue);
            }

            streaming = ! gCamera->streamStopped();
        }
    }

    gtk_widget_set_sensitive (pLink->m_maxPacketSize, supported && !streaming);
    gtk_widget_set_sensitive (pLink->m_maxPacketSizeOneTime, supported && enabled && !streaming);

    return false;  //  Only run once....
}

//
// Make Bandwidth Limit controls unselectable
static gboolean BwLimitDeactivate (gpointer pData)
{
    PxLLink *pLink = (PxLLink *)pData;

    gtk_widget_set_sensitive (pLink->m_bwLimitEnable, false);

    pLink->m_bwLimitSlider->deactivate();

    return false;  //  Only run once....
}

//
// Make Bandwidth Limit controls selectable (if appropriate)
static gboolean BwLimitActivate (gpointer pData)
{
    PxLLink *pLink = (PxLLink *)pData;

    bool supported = false;
    bool enabled = false;

    PxLAutoLock lock(&gCameraLock);

    if (gCamera)
    {
        if (gCamera->supported(FEATURE_BANDWIDTH_LIMIT))
        {
            float min, max, value;

            supported = true;
            enabled = gCamera->enabled (FEATURE_BANDWIDTH_LIMIT);

            gCamera->getRange(FEATURE_BANDWIDTH_LIMIT, &min, &max);
            pLink->m_bwLimitSlider->setRange(min, max);
            gCamera->getValue(FEATURE_BANDWIDTH_LIMIT, &value);
            pLink->m_bwLimitSlider->setValue(value);

            // Update the Bandwidth Limit label if it is limiting the frame rate
            bool warningRequired = gCamera->actualFrameRatelimiter() == FR_LIMITER_BANDWIDTH_LIMIT;
            if (warningRequired)
            {
                gtk_label_set_text (GTK_LABEL (pLink->m_bwLimitLabel), "Bandwidth Limit (Mbps) ** WARNING:Limits Frame Rate ** ");
            } else {
                gtk_label_set_text (GTK_LABEL (pLink->m_bwLimitLabel), "Bandwidth Limit (Mbps)");
            }

        }
    }

    gtk_widget_set_sensitive (pLink->m_bwLimitEnable, supported);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(pLink->m_bwLimitEnable), supported && enabled);
    pLink->m_bwLimitSlider->activate (supported && enabled);

    return false;  //  Only run once....
}

//
// Called periodically when doing onetime MaxPacketSize updates -- reads the current value
static PXL_RETURN_CODE GetCurrentMaxPacketSize()
{
    PXL_RETURN_CODE rc = ApiSuccess;

    PxLAutoLock lock(&gCameraLock);
    if (gCamera && gLinkTab)
    {
        float maxPacketSize = 0.0f;

        rc = gCamera->getValue(FEATURE_MAX_PACKET_SIZE, &maxPacketSize);
        gLinkTab->m_maxPacketSizeLast = (U32)maxPacketSize;
    }

    return rc;
}

//
// Called periodically when doing continuous maxPacketSize -- updates the user controls
static void UpdateMaxPacketSizeControls()
{
    if (gCamera && gLinkTab)
    {
        PxLAutoLock lock(&gCameraLock);

        char cValue[40];
        sprintf (cValue, "%d",(U32)gLinkTab->m_maxPacketSizeLast);
        gtk_entry_set_text (GTK_ENTRY (gLinkTab->m_maxPacketSize), cValue);

        bool onetimeMaxPacketSizeOn = false;
        if (gCamera->m_poller->polling (maxPacketSizeFuncs))
        {
            gCamera->getOnetimeAuto (FEATURE_MAX_PACKET_SIZE, &onetimeMaxPacketSizeOn);
        }
        if (!onetimeMaxPacketSizeOn)
        {
            // No need to poll any longer
            gCamera->m_poller->pollRemove(maxPacketSizeFuncs);

            // Update with the final value
            float value;
            if (API_SUCCESS(gCamera->getValue(FEATURE_MAX_PACKET_SIZE, &value)))
            {
                char cValue[40];
                sprintf (cValue, "%d",(U32)value);
                gtk_entry_set_text (GTK_ENTRY (gLinkTab->m_maxPacketSize), cValue);
            }
        }
    }
}


/* ---------------------------------------------------------------------------
 * --   Control functions from the Glade project
 * ---------------------------------------------------------------------------
 */

extern "C" void BwLimitEnableToggled
    (GtkWidget* widget, GdkEventExpose* event, gpointer userdata )
{
    if (! gCamera || !gLinkTab) return;
    if (gLinkTab->m_numRefreshRequestsOutstanding) return;

    bool enable = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON(gLinkTab->m_bwLimitEnable));

    PxLAutoLock lock(&gCameraLock);

    if (enable)
    {
        float currentValue;

        currentValue = gLinkTab->m_bwLimitSlider->getScaleValue();
        gLinkTab->m_bwLimitSlider->activate(true);
        gCamera->setValue (FEATURE_BANDWIDTH_LIMIT, currentValue);
    } else {
        gCamera->disable(FEATURE_BANDWIDTH_LIMIT);
        gLinkTab->m_bwLimitSlider->activate(false);
    }

    gLinkTab->m_bwLimitSlider->activate (enable);

    // Update our Bandwidth, as it may now be limiting the frame rate
    gdk_threads_add_idle ((GSourceFunc)BwLimitActivate, gLinkTab);

    // Notify other tabs that may be affected by this change
    gControlsTab->refreshRequired(false);
}

extern "C" void BwLimitValueChanged
    (GtkWidget* widget, GdkEventExpose* event, gpointer userdata )
{
    if (! gCamera || !gLinkTab) return;
    if (gLinkTab->m_numRefreshRequestsOutstanding) return;

    float newValue;

    newValue = gLinkTab->m_bwLimitSlider->getEditValue();
    bool enabled = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON(gLinkTab->m_bwLimitEnable));

    if (enabled)
    {
        PxLAutoLock lock(&gCameraLock);

        PXL_RETURN_CODE rc = gCamera->setValue(FEATURE_BANDWIDTH_LIMIT, newValue);

        // Read it back to see if the camera did any 'rounding'
        if (API_SUCCESS(rc))
        {
            gCamera->getValue(FEATURE_BANDWIDTH_LIMIT, &newValue);
        }

        // Notify other tabs that may be affected by this change
        gControlsTab->refreshRequired(false);
    }

    gLinkTab->m_bwLimitSlider->setValue(newValue);

    // Update our Bandwidth, as it may now be limiting the frame rate
    gdk_threads_add_idle ((GSourceFunc)BwLimitActivate, gLinkTab);
}

extern "C" void BwLimitScaleChanged
    (GtkWidget* widget, GdkEventExpose* event, gpointer userdata )
{

    if (! gCamera || !gLinkTab) return;
    if (gLinkTab->m_numRefreshRequestsOutstanding) return;

    // we are only interested in changes to the scale from user input
    if (gLinkTab->m_bwLimitSlider->rangeChangeInProgress()) return;
    if (gLinkTab->m_bwLimitSlider->setIsInProgress()) return;

    float newValue;
    bool enabled = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON(gLinkTab->m_bwLimitEnable));

    newValue = gLinkTab->m_bwLimitSlider->getScaleValue();

    if (enabled)
    {
        PxLAutoLock lock(&gCameraLock);

        PXL_RETURN_CODE rc = gCamera->setValue(FEATURE_BANDWIDTH_LIMIT, newValue);

        // Read it back to see if the camera did any 'rounding'
        if (API_SUCCESS(rc))
        {
            gCamera->getValue(FEATURE_BANDWIDTH_LIMIT, &newValue);
        }

        // Notify other tabs that may be affected by this change
        gControlsTab->refreshRequired(false);
    }

    gLinkTab->m_bwLimitSlider->setValue(newValue);

    // Update our Bandwidth, as it may now be limiting the frame rate
    gdk_threads_add_idle ((GSourceFunc)BwLimitActivate, gLinkTab);
}

extern "C" void MaxPacketSizeValueChanged
    (GtkWidget* widget, GdkEventExpose* event, gpointer userdata )
{
    if (! gCamera || ! gLinkTab) return;
    if (gLinkTab->m_numRefreshRequestsOutstanding) return;

    PxLAutoLock lock(&gCameraLock);

    U32 newMaxPacketsize = atoi (gtk_entry_get_text (GTK_ENTRY (gLinkTab->m_maxPacketSize)));
    float newValue = (float) newMaxPacketsize;
    PXL_RETURN_CODE rc;

    rc = gCamera->setValue(FEATURE_MAX_PACKET_SIZE, newValue);
    if (API_SUCCESS (rc))
    {
        // Read it back again, just in case the camera did some 'tuning' of the values
        rc = gCamera->getValue(FEATURE_MAX_PACKET_SIZE, &newValue);

        if (API_SUCCESS (rc))
        {
            newMaxPacketsize = (U32)newValue;
        }

    }

    // Reassert the current value even if we did not succeed in setting it
    char cValue[40];
    sprintf (cValue, "%d",newMaxPacketsize);
    gtk_entry_set_text (GTK_ENTRY (gLinkTab->m_maxPacketSize), cValue);
}

extern "C" void MaxPacketSizeOneTimeButtonPressed
    (GtkWidget* widget, GdkEventExpose* event, gpointer userdata )
{
    if (! gCamera || ! gCameraSelectTab || ! gLinkTab) return;
    if (gLinkTab->m_numRefreshRequestsOutstanding) return;

    PxLAutoLock lock(&gCameraLock);

    gOnetimeDialog->initiate(FEATURE_MAX_PACKET_SIZE, 250); // Pool every 250 ms
    // Also add a poller so that the edit control also update as the
    // one time is performed.
    gCamera->m_poller->pollAdd(maxPacketSizeFuncs);

}



