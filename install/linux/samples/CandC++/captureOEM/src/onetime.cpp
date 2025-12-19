
/***************************************************************************
 *
 *     File: onetime.cpp
 *
 *     Description:
 *       Perfroms a onetime (feature onepush) operation on the camera.  This
 *       includes the management of the onetime popup dialog.
*/

#include <gtk/gtk.h>
#include "onetime.h"
#include "camera.h"
#include "captureOEM.h"

extern PxLOnetime      *gOnetimeDialog;
extern GtkWindow       *gTopLevelWindow;

// prototypes to allow top dow design
static gboolean OnetimePoll (gpointer pData);

/* ---------------------------------------------------------------------------
 * --   Member functions - Public
 * ---------------------------------------------------------------------------
 */
PxLOnetime::PxLOnetime (GtkBuilder *builder)
: m_onetimeThreadRunning(false)
{
    m_windowOnetime = GTK_WIDGET( gtk_builder_get_object( builder, "windowOnetime" ) );
    gtk_window_set_transient_for (GTK_WINDOW (m_windowOnetime), gTopLevelWindow);  // Bugzilla.1316

    m_featureCtl = GTK_WIDGET( gtk_builder_get_object( builder, "OnetimeFeature_Label" ) );
    m_pollCtl    = GTK_WIDGET( gtk_builder_get_object( builder, "OnetimePoll_Label" ) );
    m_valueCtl   = GTK_WIDGET( gtk_builder_get_object( builder, "OnetimeValue_Label" ) );

}

PxLOnetime::~PxLOnetime ()
{
}

//
// NOTES:
//   - Assumes the connected camera supports onetime operation of the feature
void PxLOnetime::initiate (ULONG feature, ULONG pollInterval, float min, float max)
{
    // This can only be done if there isn't a onetime already in progress
    if (m_onetimeThreadRunning) return;

    m_feature = feature;
    m_pollInterval = pollInterval;
    m_pollNum = 0;
    m_onetimeThreadRunning = false;


    PxLAutoLock lock(&gCameraLock);
    if (gCamera)
    {
        // Use the limits if the user supplied them
        if (min < max)
        {
            gCamera->setOnetimeAuto (m_feature, true, min, max);

        } else {
            gCamera->setOnetimeAuto (m_feature, true);
        }
        // check it again right away, just in case it finished really quickly
        bool stillRunning = false;
        gCamera->getOnetimeAuto (m_feature, &stillRunning);
        if (stillRunning)
        {
            // Show the dialog
            float currentValue = 0.0f;
            gCamera->getValue (m_feature, &currentValue);
            updateDialog (m_feature, m_pollNum, currentValue);
            // Start the thread to do the periodic polls
            m_onetimeThreadRunning = true;
            gdk_threads_add_timeout (m_pollInterval, (GSourceFunc)OnetimePoll, this);
            gtk_window_present(GTK_WINDOW (m_windowOnetime));
            //gtk_widget_show (m_windowOnetime);
        }
    }
}

void PxLOnetime::updateDialog(ULONG feature, ULONG pollNum, float value)
{
    char cValue[40];

    gtk_label_set_text (GTK_LABEL (m_featureCtl), featureStr(m_feature));
    sprintf (cValue, "%d", pollNum);
    gtk_label_set_text (GTK_LABEL (m_pollCtl), cValue);
    sprintf (cValue, "%5.3f", value);
    gtk_label_set_text (GTK_LABEL (m_valueCtl), cValue);
}

/* ---------------------------------------------------------------------------
 * --   gtk thread callbacks - used to update controls
 * ---------------------------------------------------------------------------
 */

static gboolean OnetimePoll (gpointer pData)
{
    PxLOnetime *dialog = (PxLOnetime *)pData;
    dialog->m_pollNum++;

    PxLAutoLock lock(&gCameraLock);
    if (!gCamera)
    {
        // The camera is gone!
        if (dialog->m_onetimeThreadRunning)
        {
            dialog->m_onetimeThreadRunning = false;
            gtk_widget_hide (dialog->m_windowOnetime);
        }
        return false;
    }

    if (dialog->m_onetimeThreadRunning)
    {
        bool stillRunning = false;
        gCamera->getOnetimeAuto (dialog->m_feature, &stillRunning);
        if (stillRunning)
        {
            float currentValue = 0.0f;
            gCamera->getValue (dialog->m_feature, &currentValue);
            dialog->updateDialog (dialog->m_feature, dialog->m_pollNum, currentValue);
        } else {
            gtk_widget_hide (gOnetimeDialog->m_windowOnetime);
        }
        dialog->m_onetimeThreadRunning = stillRunning;
    }

    return dialog->m_onetimeThreadRunning;
}

/* ---------------------------------------------------------------------------
 * --   Control functions from the Glade project
 * ---------------------------------------------------------------------------
 */

extern "C" void CancelOnetime
    (GtkWidget* widget, GdkEventExpose* event, gpointer userdata )
{
    if (gOnetimeDialog)
    {
        PxLAutoLock lock(&gCameraLock);

        gCamera->setOnetimeAuto (gOnetimeDialog->m_feature, false);

        gOnetimeDialog->m_onetimeThreadRunning = false;
        gtk_widget_hide (gOnetimeDialog->m_windowOnetime);
        return;
    }
}



