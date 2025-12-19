/***************************************************************************
 *
 *     File: CaptureOEMLite.cpp
 *
 *     Description:
 *       This demonstrates how to create a simple camera GUI application for
 *       Linux.  It uses GTK+3 as the base GUI library, with glade to create
 *       the initial graphical layout.  Furthermore, it uses gthreads to create
 *       a separate thread to scan for camera connects/disconnects.
 *
 *     Design Notes:
 *        - This design uses a very simple, 'open' concept of our C++ data objects.
 *          Each of the objects are actually global within the application.  This
 *          simple approach can easily deal with:
 *            o we are using the C variant of glade, with the 'signals' associated
 *              with each of the GUI controls, mapped to a global function
 *            o There is a significant number of 'interdependencies' amongst the
 *              controls; changes the user makes to one control, often has impact on
 *              what is represented in another control
 *        - All of the camera interactions, and their calls to the PixeLINK API,
 *          are in the camera.cpp module.
 *     
 *     Revision History:
 *       Version   Date          Description
 *       -------   ----          -----------
 *       1.0       2015-08-04    Initial release
 *       1.1       2017-02-15    Bugzilla.952 - slider edit fields now updated on loss of focus
 *       1.2       2017-04-26    Bugzilla.995 - Only update the frame rate if it has changed
 *                 2017-04-27    Added support for FEATURE_FOCUS
 *                 2017-04-28    Added a wait cursor for one-time auto operations
 */


#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <vector>

#include "camera.h"
#include "cameraSelectCombo.h"
#include "previewButtons.h"
#include "exposure.h"
#include "pixelFormat.h"
#include "pixelAddress.h"
#include "frameRate.h"
#include "gain.h"
#include "focus.h"
#include "whiteBalance.h"
#include "imageFlip.h"
#include "imageCapture.h"

using namespace std;

//
// Useful defines and enums.
//
#define ASSERT(x)	do { assert((x)); } while(0)

// Prototypes
extern "C" void StopButtonPressed
  (GtkWidget* widget, GdkEventExpose* event, gpointer userdata);

// The currently selected camera.  A NULL value indicates not camera has been selected.  Note that
//       This 'global' is accessed from multiple threads.  In particular, the active camera can be
//       removed and redefined by the camera scanThread.  We will use a mutex to proect ourselves
//       from issues that could otherwise happen.  Users of pCamera, should grab the mutex first.  The
//       class PxLAutoLock is a convenient way to do this.
PxLCamera        *pCamera = NULL;
pthread_mutex_t   pCameraLock;

// Our GUI control objects.
GtkWindow *topLevelWindow;
PxLCameraSelectCombo *csObject = NULL;
PxLPreviewButtons *previewObject = NULL;
PxLExposure *exposureObject = NULL;
PxLPixelFormat *pixelFormatObject = NULL;
PxLPixelAddress *pixelAddressObject = NULL;
PxLRoi *roiObject = NULL;
PxLFrameRate *frameRateObject = NULL;
PxLGain *gainObject = NULL;
PxLFocus *focusObject = NULL;
PxLWhiteBalance *whiteBalanceObject = NULL;
PxLImageFlip *imageFlipObject = NULL;
PxLImageCapture *imageCaptureObject = NULL;


int main(int argc, char* argv[]) {

    GtkBuilder *builder;
    GtkWidget  *window;
    GError     *error = NULL;
    pthread_mutexattr_t mutexAttr;


    //
    // Step 1.
    //    Initialize GTK+3. This includes reading our glade generated XML file that deifnes our
    //    user interface.
    gtk_init( &argc, &argv );

    builder = gtk_builder_new();
    // Load UI from file. If error occurs, report it and quit application.
    if( ! gtk_builder_add_from_file( builder, "C-OEMLite.glade", &error ) )
    {
        g_warning( "%s", error->message );
        return( 1 );
    }

    // Get the main window pointer from the UI
    window = GTK_WIDGET( gtk_builder_get_object( builder, "windowMain" ) );
    topLevelWindow = GTK_WINDOW (window);

    //
    // Step 2.
    //      Initialize the mutex lock we use for our global pCamera
    pthread_mutexattr_init (&mutexAttr);
    pthread_mutexattr_settype (&mutexAttr, PTHREAD_MUTEX_RECURSIVE);
    pthread_mutex_init (&pCameraLock, &mutexAttr);

    //
    // Step 3.
    //      Create all of our UI control object.  They do all of the real work.
    csObject = new PxLCameraSelectCombo (GTK_WIDGET( gtk_builder_get_object( builder, "CameraSelect_ComboBoxText" ) ) );
    previewObject = new PxLPreviewButtons
    						( GTK_WIDGET( gtk_builder_get_object( builder, "Play_Button" ) ),
    						  GTK_WIDGET( gtk_builder_get_object( builder, "Pause_Button" ) ),
    						  GTK_WIDGET( gtk_builder_get_object( builder, "Stop_Button" ) ),
    						  GTK_WIDGET( gtk_builder_get_object( builder, "ResizePreviewToRoi_Button" ) ));
    exposureObject = new PxLExposure
            ( GTK_WIDGET( gtk_builder_get_object( builder, "ExposureMin_Label" ) ),
              GTK_WIDGET( gtk_builder_get_object( builder, "ExposureMax_Label" ) ),
              GTK_WIDGET( gtk_builder_get_object( builder, "Exposure_Scale" ) ),
              GTK_WIDGET( gtk_builder_get_object( builder, "Exposure_Text" ) ),
              GTK_WIDGET( gtk_builder_get_object( builder, "ExposureOneTime_Button" ) ),
              GTK_WIDGET( gtk_builder_get_object( builder, "ExposureContinuous_Checkbutton" ) ) );
    pixelFormatObject = new PxLPixelFormat
            ( GTK_WIDGET( gtk_builder_get_object( builder, "PixelFormat_ComboBoxText" ) ) );
    pixelAddressObject = new PxLPixelAddress
            (GTK_WIDGET( gtk_builder_get_object( builder, "PixelAddressMode_ComboBoxText" ) ),
             GTK_WIDGET( gtk_builder_get_object( builder, "PixelAddressValue_ComboBoxText" ) ) );
    roiObject = new PxLRoi
            ( GTK_WIDGET( gtk_builder_get_object( builder, "ROI_ComboBoxText" ) ) );
    frameRateObject = new PxLFrameRate
            ( GTK_WIDGET( gtk_builder_get_object( builder, "FrameRateMin_Label" ) ),
              GTK_WIDGET( gtk_builder_get_object( builder, "FrameRateMax_Label" ) ),
              GTK_WIDGET( gtk_builder_get_object( builder, "FrameRate_Scale" ) ),
              GTK_WIDGET( gtk_builder_get_object( builder, "FrameRate_Text" ) ),
              GTK_WIDGET( gtk_builder_get_object( builder, "FrameRateOneTime_Button" ) ),
              GTK_WIDGET( gtk_builder_get_object( builder, "FrameRateContinuous_Checkbutton" ) ) );
    gainObject = new PxLGain
            ( GTK_WIDGET( gtk_builder_get_object( builder, "GainMin_Label" ) ),
              GTK_WIDGET( gtk_builder_get_object( builder, "GainMax_Label" ) ),
              GTK_WIDGET( gtk_builder_get_object( builder, "Gain_Scale" ) ),
              GTK_WIDGET( gtk_builder_get_object( builder, "Gain_Text" ) ),
              GTK_WIDGET( gtk_builder_get_object( builder, "GainOneTime_Button" ) ),
              GTK_WIDGET( gtk_builder_get_object( builder, "GainContinuous_Checkbutton" ) ) );
    focusObject = new PxLFocus
            ( GTK_WIDGET( gtk_builder_get_object( builder, "FocusMin_Label" ) ),
              GTK_WIDGET( gtk_builder_get_object( builder, "FocusMax_Label" ) ),
              GTK_WIDGET( gtk_builder_get_object( builder, "Focus_Scale" ) ),
              GTK_WIDGET( gtk_builder_get_object( builder, "Focus_Text" ) ),
              GTK_WIDGET( gtk_builder_get_object( builder, "FocusOneTime_Button" ) ),
              GTK_WIDGET( gtk_builder_get_object( builder, "FocusContinuous_Checkbutton" ) ) );
    whiteBalanceObject = new PxLWhiteBalance
            ( GTK_WIDGET( gtk_builder_get_object( builder, "RedMin_Label" ) ),
              GTK_WIDGET( gtk_builder_get_object( builder, "RedMax_Label" ) ),
              GTK_WIDGET( gtk_builder_get_object( builder, "Red_Scale" ) ),
              GTK_WIDGET( gtk_builder_get_object( builder, "Red_Text" ) ),
              GTK_WIDGET( gtk_builder_get_object( builder, "GreenMin_Label" ) ),
              GTK_WIDGET( gtk_builder_get_object( builder, "GreenMax_Label" ) ),
              GTK_WIDGET( gtk_builder_get_object( builder, "Green_Scale" ) ),
              GTK_WIDGET( gtk_builder_get_object( builder, "Green_Text" ) ),
              GTK_WIDGET( gtk_builder_get_object( builder, "BlueMin_Label" ) ),
              GTK_WIDGET( gtk_builder_get_object( builder, "BlueMax_Label" ) ),
              GTK_WIDGET( gtk_builder_get_object( builder, "Blue_Scale" ) ),
              GTK_WIDGET( gtk_builder_get_object( builder, "Blue_Text" ) ),
              GTK_WIDGET( gtk_builder_get_object( builder, "WhiteBalanceOneTime_Button" ) ) );
    imageFlipObject = new PxLImageFlip
            ( GTK_WIDGET( gtk_builder_get_object( builder, "ImageFlipX_Checkbutton" ) ),
              GTK_WIDGET( gtk_builder_get_object( builder, "ImageFlipY_Checkbutton" ) ) );
    imageCaptureObject = new PxLImageCapture
            ( GTK_WIDGET( gtk_builder_get_object( builder, "ImageCaptureName_Text" ) ),
              GTK_WIDGET( gtk_builder_get_object( builder, "ImageCapture_FileChooserbutton" ) ),
              GTK_WIDGET( gtk_builder_get_object( builder, "ImageCapture_CcomboBoxText" ) ),
              GTK_WIDGET( gtk_builder_get_object( builder, "ImageCapture_Button" ) ) );

    //
    // Step 4.
    //      Map our glade specified 'signals' to our control objects.
    gtk_builder_connect_signals( builder, NULL );
    // We don't need the builder anymore, so destroy it.
    g_object_unref( G_OBJECT( builder ) );

    //
    // Step 5.
    //      Show the window and transfer control to GTK+3
    gtk_widget_show( window );
    // and off we go....
    gtk_main();

    //
    // Step 6.
    //      User wants to quit.  Do cleanup and exit.
    delete csObject;
    delete previewObject;
    delete exposureObject;
    delete pixelFormatObject;
    delete pixelAddressObject;
    delete roiObject;
    delete frameRateObject;
    delete gainObject;
    delete focusObject;
    delete whiteBalanceObject;
    delete imageFlipObject;
    delete imageCaptureObject;

    pthread_mutex_destroy (&pCameraLock);

    return (0);
}

extern "C" void ApplicationQuit
    (GtkWidget* widget, GdkEventExpose* event, gpointer userdata )
{
	// The user is quitting the application.  'Simulate' a press of the
	// stop button first, just in case they left the camera
	// streaming.
	StopButtonPressed (NULL, NULL, NULL);

	gtk_main_quit ();
}
