/***************************************************************************
 *
 *     File: CaptureOEM.cpp
 *
 *     Description:
 *       Demonstration GUI  application to control a PixeLINK camera using
 *       the PixeLINK API. It uses GTK+3 as the base GUI library, with glade
 *       to create the initial graphical layout.  Furthermore, it uses
 *       gthreads to create a separate thread to scan for camera
 *       connects/disconnects.
 *
 *     Design Notes:
 *        - All of the camera interactions, and their calls to the PixeLINK API,
 *          are in the camera.cpp module.
 *     
 *     Revision History:
 *       Version   Date          Description
 *       -------   ----          -----------
 *       1.0.0     2017-12-14    Creation
 *       1.0.1     2018-05-02    Bugzilla.1335 - Segfaults can occur when a camera is de-selected
 *                               Bugzilla.1336 - OneTime auto button can enabled when it should not be (no stream)
 *       1.0.2     2018-05-15    Bugzilla.1337 - One time auto exposure does not work in some circumstances
 *       1.0.3     2018-11-20    Added support for Polar and Gain based HDR cameras
 *                 2019-02-19    Bugzilla.1592 - Gain slider is not updated with continuous value sometimes
 *                               Bugzilla.1593 - Trigger mode 14 does not work correctly
 *                 2019-02-20    Bugzilla.1596 - OneTime white balance button is not disabled for mono camera
 *       1.0.4     2019-02-21    Bugzilla.1599 - Removed some debug messages
 *                 2019-02-22    Bugzilla.1597 - Video playback time sometimes not displayed correctly
 *                 2019-02-22    Bugzilla.1600 - Frame rate slider limits are not updated when the user changes HDR mode
 *       1.0.5     2019-03-01    Bugzilla.1613 - Frame Rate and Exposure not updated when ROI is changed via drag, in FFM
 *       1.0.6     2019-09-18    Added support for RGB8 / BGR8 (AKA RGB23/BGR24)
 *       1.0.7     2020-02-06    Added support for PL-X (10 GigE) cameras
 *                 2020-02-12    Bugzilla.1858 - Don't update the controls while a new camera is being selected
 *                 2020-05-06    Bugzilla.1932 - Video capture warning should be on top of progress dialog
 *       1.1.0     2020-10-28    Bugzilla.2138 - GTK warning messages when a new camera is selected
 *                 2020-10-28    Bugzilla.2139 - Pop-up error when selecting GPI (GPIO #3 on a PL-X)
 *       		   2020-10-28    Added controls for Events, Actions, and PTP
 *       1.1.1     2020-12-09    Bugzilla.2174 - Selecting 'No Camera' while streaming, may cause and abend.
 *       1.1.2     2020-12-16    Bugzilla.2179 - Switching cameras while streaming may cause abend
 *                 2020-12-16    Bugzilla.2180 - Gain controls should be disabled when in HDR mode
 *                 2020-12-16    Bugzilla.2183 - Max packet size controls should be disabled while streaming
 *       1.1.3     2021-01-04    Bugzilla.2188 - Cleanup of trigger operation
 *                 2021-01-04    Bugzilla.2187 - Added auto limits for the auto focus operations (if supported)
 *                 2021-01-05    Bugzilla.2182 - Don't accept negative numbers for video recording options
 *       1.1.4     2021-02-10    Added (non)support for API Lite (new as of SDK v3.1) -- An error is displayed if the Lite variant is used
 *       1.1.5     2021-10-05    Bugzilla.2389 - Add capability to save cameras settings to a file
 *       1.1.6     2021-11-01    Bugzilla.2400 - Small improvement to captureOem edit fields
 *       1.1.7     2022-01-20    Added support for the 'alpha' pixel formats (RGBA/BGRA/ARGB/ABGR)
 *                 2022-02-18    Added support for EVENT_SENSOR_SYNCHRONIZED and ACTION_SENSOR_SYNC
 *       1.1.8     2023-01-24    Bugzilla.2582 - Can get in an error loop if the user attempts to enable a GPIO as input, if input is not supported
 *                 2023-02-03    Added support for FEATURE_BRIGHTNESS
 *       1.1.9     2024-02-15    Added support for FEATURE_COMPRESSION
 *       1.1.10    2024-11-18    bugzilla.2913 - Removed video capture
 */

char CaptureOEMVersion[] = "1.1.10";

#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <vector>

#include "camera.h"
#include "cameraSelect.h"
#include "preview.h"
#include "controls.h"
#include "stream.h"
#include "image.h"
#include "video.h"
#include "gpio.h"
#include "videoCaptureDialog.h"
#include "lens.h"
#include "roi.h"
#include "filter.h"
#include "link.h"
#include "autoRoi.h"
#include "cameraTime.h"
#include "info.h"

#include "onetime.h"
#include "captureOEM.h"

using namespace std;

//
// Useful defines and enums.
//
#define ASSERT(x)	do { assert((x)); } while(0)

// Local prototypes to permit top-down design
static bool loadCss(const gchar *filename, GError **error);
extern "C" void StopButtonPressed (GtkWidget* widget, GdkEventExpose* event, gpointer userdata);
static void defineStandardROIs();

std::vector<PXL_ROI> PxLStandardRois;


// The currently selected camera.  A NULL value indicates no camera has been selected.  Note that
//       This 'global' is accessed from multiple threads.  In particular, the active camera can be
//       removed and redefined by the camera scanThread.  We will use a mutex to protect ourselves
//       from issues that could otherwise happen.  Users of pCamera, should grab the mutex first.  The
//       class PxLAutoLock is a convenient way to do this.
PxLCamera        *gCamera = NULL;
pthread_mutex_t   gCameraLock;


// Our GUI control objects -- each tab has it's own object.
GtkWindow *gTopLevelWindow;
GtkWidget *captureOEMNotebook;

// Each of the applications control groups.  Generally, each group is a 'tab' within the application
PxLCameraSelect *gCameraSelectTab = NULL;
PxLPreview      *gVideoPreviewTab = NULL;
PxLControls     *gControlsTab = NULL;
PxLStream       *gStreamTab = NULL;
PxLImage        *gImageTab = NULL;
#if defined VIDEO_CAPTURE_SUPPORTED
PxLVideo        *gVideoTab = NULL;
#endif
PxLGpio         *gGpioTab = NULL;
PxLLens         *gLensTab = NULL;
PxLFilter       *gFilterTab = NULL;
PxLLink         *gLinkTab = NULL;
PxLAutoRoi      *gAutoRoiTab = NULL;
PxLTime         *gTimeTab = NULL;
PxLInfo         *gInfoTab = NULL;

PxLOnetime      *gOnetimeDialog = NULL;
#if defined VIDEO_CAPTURE_SUPPORTED
PxLVideoCaptureDialog *gVideoCaptureDialog = NULL;
#endif

C_OEM_TABS  gCurrentTab = ControlsTab;

int main(int argc, char* argv[]) {

    GtkBuilder *builder;
    GtkWidget  *windowMain;
    GError     *error = NULL;
    pthread_mutexattr_t mutexAttr;

    //
    // Step 1.
    //    Initialize GTK+3. This includes reading our style sheet and the glade generated XML file
    //    that defines our user interface.
    gtk_init( &argc, &argv );

    // Step 1.1
    //    Read the style sheet
    if (!loadCss("captureOEM.css", &error))
    {
        g_warning( "%s", error->message );
        return( EXIT_FAILURE );
    }

    // Step 1.2
    //    Load our (Glade) UI file
    builder = gtk_builder_new();
    // Load UI from file. If error occurs, report it and quit application.
    if( ! gtk_builder_add_from_file( builder, "captureOEM.glade", &error ) )
    {
        g_warning( "%s", error->message );
        return( EXIT_FAILURE );
    }

    // Step 1.3
    //    Create our main window
    // Get the main window pointer from the UI
    windowMain = GTK_WIDGET( gtk_builder_get_object( builder, "windowMain" ) );
    gTopLevelWindow = GTK_WINDOW (windowMain);

    // Set the default window size.  We chose this size so the the window is just large enough to
    // not require scrolling.  If the user makes the window smaller, scroll bars will appear
    gtk_window_set_default_size (gTopLevelWindow, 768, 896);

    //
    // Step 3.
    //      Define our 'standard ROIs'
    defineStandardROIs();

    //
    // Step 2.
    //      Initialize the mutex lock we use for our global pCamera
    pthread_mutexattr_init (&mutexAttr);
    pthread_mutexattr_settype (&mutexAttr, PTHREAD_MUTEX_RECURSIVE);
    pthread_mutex_init (&gCameraLock, &mutexAttr);

    //
    // Step 3.
    //      Create all of our UI control objects.  They do all of the real work.
    captureOEMNotebook = GTK_WIDGET( gtk_builder_get_object( builder, "CaptureOEM_Notebook" ) );
    gCameraSelectTab = new PxLCameraSelect(builder);
    gVideoPreviewTab = new PxLPreview(builder);
    gControlsTab     = new PxLControls(builder);
    gStreamTab       = new PxLStream(builder);
    gImageTab        = new PxLImage(builder);
#if defined VIDEO_CAPTURE_SUPPORTED
    gVideoTab        = new PxLVideo(builder);
#endif
    gGpioTab         = new PxLGpio(builder);
    gLensTab         = new PxLLens(builder);
    gFilterTab       = new PxLFilter(builder);
    gLinkTab         = new PxLLink(builder);
    gAutoRoiTab      = new PxLAutoRoi(builder);
    gTimeTab         = new PxLTime(builder);
    gInfoTab         = new PxLInfo(builder);

    gOnetimeDialog   = new PxLOnetime(builder);
#if defined VIDEO_CAPTURE_SUPPORTED
    gVideoCaptureDialog = new PxLVideoCaptureDialog(builder);
#endif

    //
    // Step 4.
    //      Map our glade specified 'signals' to our control objects.
    gtk_builder_connect_signals( builder, NULL );
    // We don't need the builder anymore, so destroy it.
    g_object_unref( G_OBJECT( builder ) );

    //
    // Step 5.
    //      Show the window and transfer control to GTK+3
    gtk_widget_show( windowMain );
    // and off we go....
    gtk_main();

    //
    // Step 6.
    //      User wants to quit.  Do cleanup and exit.
#if defined VIDEO_CAPTURE_SUPPORTED
    delete gVideoCaptureDialog; gVideoCaptureDialog = NULL;
#endif
    delete gOnetimeDialog; gOnetimeDialog = NULL;

    delete gInfoTab; gInfoTab = NULL;
    delete gTimeTab; gTimeTab = NULL;
    delete gAutoRoiTab; gAutoRoiTab = NULL;
    delete gLinkTab; gLinkTab = NULL;
    delete gFilterTab; gFilterTab = NULL;
    delete gLensTab; gLensTab = NULL;
    delete gGpioTab; gGpioTab = NULL;
#if defined VIDEO_CAPTURE_SUPPORTED
    delete gVideoTab; gVideoTab = NULL;
#endif
    delete gImageTab; gImageTab = NULL;
    delete gStreamTab; gStreamTab = NULL;
    delete gControlsTab; gControlsTab = NULL;
    delete gVideoPreviewTab; gVideoPreviewTab = NULL;
    delete gCameraSelectTab; gCameraSelectTab = NULL;

    pthread_mutex_destroy (&gCameraLock);

    return (EXIT_SUCCESS);
}

/* ---------------------------------------------------------------------------
 * --   Local functions
 * ---------------------------------------------------------------------------
 */

//
// Define the standard ROIs used by C-OEM
static void defineStandardROIs()
{
    PxLStandardRois.push_back(PXL_ROI( 32,     32    ));
    PxLStandardRois.push_back(PXL_ROI( 64,     64    ));
    PxLStandardRois.push_back(PXL_ROI( 128,    128   ));
    PxLStandardRois.push_back(PXL_ROI( 256,    256   ));
    PxLStandardRois.push_back(PXL_ROI( 320,    240   )); // QVGA
    PxLStandardRois.push_back(PXL_ROI( 640,    480   )); // VGA, or SD
    PxLStandardRois.push_back(PXL_ROI( 800,    600   )); // SVGA
    PxLStandardRois.push_back(PXL_ROI( 1024,   768   )); // XGA
    PxLStandardRois.push_back(PXL_ROI( 1280,   1024  )); // SXGA
    PxLStandardRois.push_back(PXL_ROI( 1600,   1200  )); // UXGA
    PxLStandardRois.push_back(PXL_ROI( 1920,   1200  )); // WUXGA
    PxLStandardRois.push_back(PXL_ROI( 2048,   1536  )); // QXGA
    PxLStandardRois.push_back(PXL_ROI( 2560,   1600  )); // WQXGA
    PxLStandardRois.push_back(PXL_ROI( 3200,   2400  )); // QUXGA
    PxLStandardRois.push_back(PXL_ROI( 4096,   3972  )); // HXGA
}

//
// Read the style sheet for our application.  Returns 'true' on success
static bool loadCss(const gchar *filename, GError **error)
{
    GtkCssProvider *provider;
    GdkDisplay *display;
    GdkScreen *screen;

    provider = gtk_css_provider_new ();
    display = gdk_display_get_default ();
    screen = gdk_display_get_default_screen (display);
    gtk_style_context_add_provider_for_screen (screen, GTK_STYLE_PROVIDER (provider), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);


    gtk_css_provider_load_from_file (provider, g_file_new_for_path (filename), error);
    g_object_unref (provider);

    return (*error == NULL);
}


/* ---------------------------------------------------------------------------
 * --   Global functions
 * ---------------------------------------------------------------------------
 */

//
// Grabs hold of a new camera
void GrabCamera (PCAMERA_ID_INFO pNewCameraIdInfo)
{
    ASSERT (!gCamera);

    try
    {
        gCamera = new PxLCamera (pNewCameraIdInfo);

        // Let all of the control groups (tabs) know about the new camera
        gVideoPreviewTab->refreshRequired(false);
        gControlsTab->refreshRequired(false);
        gStreamTab->refreshRequired(false);
        gImageTab->refreshRequired(false);
#if defined VIDEO_CAPTURE_SUPPORTED
        gVideoTab->refreshRequired(false);
#endif
        gGpioTab->refreshRequired(false);
        gLensTab->refreshRequired(false);
        gFilterTab->refreshRequired(false);
        gLinkTab->refreshRequired(false);
        gAutoRoiTab->refreshRequired(false);
        gTimeTab->refreshRequired(false);
        gInfoTab->refreshRequired(false);

    } catch (PxLError& e) {
        switch (e.m_rc)
        {
        case ApiNoCameraError:
        case ApiNoCameraAvailableError:
            break;
        case ApiNotSupportedOnLiteVersion:
            printf ("ERROR: Pixelink Lite API detected.  This application requires the standard variant\n");
            break;
        default:
            printf ("%s\n", e.showReason());
        }
        gCamera = NULL;
    }
}

//
// Releases the currently held camera
void ReleaseCamera ()
{
    ASSERT (gCamera);

    delete gCamera;
    gCamera = NULL;

    // Let all of the control groups (tabs) know about the loss of the camera
    if (gVideoPreviewTab)gVideoPreviewTab->refreshRequired(true);
    if (gControlsTab) gControlsTab->refreshRequired(true);
    if (gStreamTab) gStreamTab->refreshRequired(true);
    if (gImageTab) gImageTab->refreshRequired(true);
#if defined VIDEO_CAPTURE_SUPPORTED
    if (gVideoTab) gVideoTab->refreshRequired(true);
#endif
    if (gGpioTab) gGpioTab->refreshRequired(true);
    if (gLensTab) gLensTab->refreshRequired(true);
    if (gFilterTab) gFilterTab->refreshRequired(true);
    if (gLinkTab) gLinkTab->refreshRequired(true);
    if (gAutoRoiTab) gAutoRoiTab->refreshRequired(true);
    if (gTimeTab) gTimeTab->refreshRequired(true);
    if (gInfoTab) gInfoTab->refreshRequired(true);
}

// returns true if 'tab' is the currently selected tab
bool IsActiveTab (C_OEM_TABS tab)
{
    gint currentTab = gtk_notebook_get_current_page (GTK_NOTEBOOK(captureOEMNotebook));

    return (tab == currentTab);
}

// returns true if 'tab' is the currently selected tab
PxLTab* GetActiveTab ()
{
    gint currentTab = gtk_notebook_get_current_page (GTK_NOTEBOOK(captureOEMNotebook));

    switch (currentTab)
    {
    default:
    case ControlsTab: return (PxLTab*)gControlsTab;
    case StreamTab:   return (PxLTab*)gStreamTab;
    case ImageTab:    return (PxLTab*)gImageTab;
#if defined VIDEO_CAPTURE_SUPPORTED
    case VideoTab:    return (PxLTab*)gVideoTab;
#endif
    case GpioTab:     return (PxLTab*)gGpioTab;
    case LensTab:     return (PxLTab*)gLensTab;
    case FilterTab:   return (PxLTab*)gFilterTab;
    case LinkTab:     return (PxLTab*)gLinkTab;
    case AutoRoiTab:  return (PxLTab*)gAutoRoiTab;
    case TimeTab:     return (PxLTab*)gTimeTab;
    case InfoTab:     return (PxLTab*)gInfoTab;
    }
}

/* ---------------------------------------------------------------------------
 * --   Control functions from the Glade project
 * ---------------------------------------------------------------------------
 */

//
// Quitting the application (such as by clicking on the red 'x')
extern "C" void ApplicationQuit
    (GtkWidget* widget, GdkEventExpose* event, gpointer userdata )
{
	// The user is quitting the application.  'Simulate' a press of the
	// stop button first, just in case they left the camera
	// streaming / preview open
	StopButtonPressed(NULL, NULL, NULL);

	gtk_main_quit ();
}

//
// User selected a new 'tab' group
extern "C" void NewTabSelected
    (GtkWidget* widget, GdkEventExpose* event, gint newTab )
{
    if (newTab == gCurrentTab) return;

    switch (gCurrentTab) {
        case ControlsTab:
            gControlsTab->deactivate();
            break;
        case StreamTab:
            gStreamTab->deactivate();
            break;
        case ImageTab:
            gImageTab->deactivate();
            break;
#if defined VIDEO_CAPTURE_SUPPORTED
        case VideoTab:
            gVideoTab->deactivate();
            break;
#endif
        case GpioTab:
            gGpioTab->deactivate();
            break;
        case LensTab:
            gLensTab->deactivate();
            break;
        case FilterTab:
            gFilterTab->deactivate();
            break;
        case LinkTab:
            gLinkTab->deactivate();
            break;
        case AutoRoiTab:
            gAutoRoiTab->deactivate();
            break;
        case TimeTab:
            gTimeTab->deactivate();
            break;
        case InfoTab:
            gInfoTab->deactivate();
            break;
        default:
            break;
    }

    switch (newTab) {
        case ControlsTab:
            gControlsTab->activate();
            break;
        case StreamTab:
            gStreamTab->activate();
            break;
        case ImageTab:
            gImageTab->activate();
            break;
#if defined VIDEO_CAPTURE_SUPPORTED
        case VideoTab:
            gVideoTab->activate();
            break;
#endif
        case GpioTab:
            gGpioTab->activate();
            break;
        case LensTab:
            gLensTab->activate();
            break;
        case FilterTab:
            gFilterTab->activate();
            break;
        case LinkTab:
            gLinkTab->activate();
            break;
        case AutoRoiTab:
            gAutoRoiTab->activate();
            break;
        case TimeTab:
            gTimeTab->activate();
        case InfoTab:
            gInfoTab->activate();
            break;
        default:
            break;
    }
    gCurrentTab = static_cast<C_OEM_TABS>(newTab);
}

//
// User selected a new 'tab' group
extern "C" void MenuRefresh
    (GtkWidget* widget, GdkEventExpose* event, gint newTab )
{

    PxLTab* currentTab = GetActiveTab ();

    // Tell the current tab that a refresh is required, This will automaitcally
    // do a refresh (re-activate) of the active tab.
    currentTab->refreshRequired(gCamera == NULL);
}

//
// User selected a new 'tab' group
extern "C" void MenuAbout
    (GtkWidget* widget, GdkEventExpose* event, gint newTab )
{

    // Change to the 'Info' tab
    gtk_notebook_set_current_page (GTK_NOTEBOOK(captureOEMNotebook), InfoTab);
}
