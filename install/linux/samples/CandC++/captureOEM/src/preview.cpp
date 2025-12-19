
/***************************************************************************
 *
 *     File: preview.cpp
 *
 *     Description:
 *         Controls for the 'Video Preview' controls
 *         in CaptureOEM.
 */

#include <PixeLINKApi.h>
#include "preview.h"
#include "camera.h"
#include "captureOEM.h"
#include "controls.h"
#include "stream.h"
#include "lens.h"
#include "link.h"

extern PxLPreview  *gVideoPreviewTab;
extern GtkWindow   *gTopLevelWindow;
extern PxLControls *gControlsTab;
extern PxLStream   *gStreamTab;
extern PxLLens     *gLensTab;
extern PxLLink     *gLinkTab;

//
// Local prototypes.
//    UI updates can only be done from a gtk thread -- these routines are gtk 'idle' threads
//    and as such are 'UI update safe'. For each 'feature', there there the following functions:
//       . {featureXXX}Deactivate - Makes the controls meaningless (including greying them out)
//       . {featreuXXX}Activate - updates the controls with values from the camera
static gboolean PreviewDeactivate (gpointer pData);
static gboolean PreviewActivate (gpointer pData);

/* ---------------------------------------------------------------------------
 * --   Member functions
 * ---------------------------------------------------------------------------
 */
PxLPreview::PxLPreview (GtkBuilder *builder)
{
	m_play  = GTK_WIDGET( gtk_builder_get_object( builder, "Play_Button" ) );
	m_pause = GTK_WIDGET( gtk_builder_get_object( builder, "Pause_Button" ) );
	m_stop  = GTK_WIDGET( gtk_builder_get_object( builder, "Stop_Button" ) );

	m_autoResize = GTK_WIDGET( gtk_builder_get_object( builder, "AutoResize_Checkbox" ) );
}

PxLPreview::~PxLPreview ()
{
}

void PxLPreview::refreshRequired (bool noCamera)  // Used to indicate whether we have a new camera, or no camera
{
    if (noCamera)
    {
        gdk_threads_add_idle ((GSourceFunc)PreviewDeactivate, this);
    } else {
        gdk_threads_add_idle ((GSourceFunc)PreviewActivate, this);
    }
}

/* ---------------------------------------------------------------------------
 * --   gtk thread callbacks - used to update controls
 * ---------------------------------------------------------------------------
 */

//
// Make preview buttons unselectable
static gboolean PreviewDeactivate (gpointer pData)
{
    PxLPreview *pControls = (PxLPreview *)pData;

    gtk_widget_set_sensitive (pControls->m_play, false);
    gtk_widget_set_sensitive (pControls->m_pause, false);
    gtk_widget_set_sensitive (pControls->m_stop, false);
    gtk_widget_set_sensitive (pControls->m_autoResize, false);

    return false;  //  Only run once....
}

//
// Make preview buttons selectable (if appropriate)
static gboolean PreviewActivate (gpointer pData)
{
    PxLPreview *pControls = (PxLPreview *)pData;

    if (gCamera)
    {
        gtk_widget_set_sensitive (pControls->m_play, true);
        gtk_widget_set_sensitive (pControls->m_autoResize, true);
    }
    gtk_widget_set_sensitive (pControls->m_pause, false);
    gtk_widget_set_sensitive (pControls->m_stop, false);

    return false;  //  Only run once....
}

/* ---------------------------------------------------------------------------
 * --   Control functions from the Glade project
 * ---------------------------------------------------------------------------
 */
extern "C" void PlayButtonPressed
    (GtkWidget* widget, GdkEventExpose* event, gpointer userdata )
{

    PxLAutoLock lock(&gCameraLock);
    if (gCamera) if (!API_SUCCESS (gCamera->play())) return;

    gtk_widget_set_sensitive (gVideoPreviewTab->m_play, false);
    gtk_widget_set_sensitive (gVideoPreviewTab->m_pause, true);
    gtk_widget_set_sensitive (gVideoPreviewTab->m_stop, true);
    gtk_widget_set_sensitive (gVideoPreviewTab->m_autoResize, true);

    // The above 'play' operation may have opened the preview on top of our
    // application.  How rude!!
    // Reassert our application on top.
    gtk_window_set_keep_above( gTopLevelWindow, true );

    gStreamTab->playChange(true);
    gLensTab->playChange(true);
    gControlsTab->streamChange(true);
    gLinkTab->streamChange(true);

}

extern "C" void PauseButtonPressed
    (GtkWidget* widget, GdkEventExpose* event, gpointer userdata )
{
    PxLAutoLock lock(&gCameraLock);
    if (gCamera) if (!API_SUCCESS (gCamera->pause())) return;

    gtk_widget_set_sensitive (gVideoPreviewTab->m_play, true);
    gtk_widget_set_sensitive (gVideoPreviewTab->m_pause, false);
    gtk_widget_set_sensitive (gVideoPreviewTab->m_stop, true);
    gtk_widget_set_sensitive (gVideoPreviewTab->m_autoResize, true);

    gControlsTab->streamChange(false);
    gLinkTab->streamChange(false);
}

extern "C" void StopButtonPressed
    (GtkWidget* widget, GdkEventExpose* event, gpointer userdata )
{
    PxLAutoLock lock(&gCameraLock);
    if (gCamera) if (!API_SUCCESS (gCamera->stop())) return;

    gtk_widget_set_sensitive (gVideoPreviewTab->m_play, true);
    gtk_widget_set_sensitive (gVideoPreviewTab->m_pause, false);
    gtk_widget_set_sensitive (gVideoPreviewTab->m_stop, false);
    gtk_widget_set_sensitive (gVideoPreviewTab->m_autoResize, true);

   // We don't need to worry about the preview window anymore, so no need to
    // keep our application on top of it.
    gtk_window_set_keep_above( gTopLevelWindow, false );

    gControlsTab->streamChange(false);
    gLinkTab->streamChange(false);
}

extern "C" void AutoResizeToggled
    (GtkWidget* widget, GdkEventExpose* event, gpointer userdata )
{
    if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON(gVideoPreviewTab->m_autoResize)))
    {
        gCamera->resizePreviewToRoi();
    }
}

static gboolean previewStop (gpointer pData)
{
    // If the user presses the little red X in the preview window, then the API
    // treats this just like the user calling PxLSetPreviewState STOP_PREVIEW.  We
    // want to make this look the same as the user having pressed the stop button.

    StopButtonPressed (NULL, NULL, NULL);

    return false; //  false == only run once
}

extern "C" U32 PreviewWindowEvent
  (HANDLE hCamera, U32 event, LPVOID pdata)
{
    if (PREVIEW_CLOSED == event)
    {
        gdk_threads_add_idle ((GSourceFunc)previewStop, NULL);
    }

    return 0;
}




