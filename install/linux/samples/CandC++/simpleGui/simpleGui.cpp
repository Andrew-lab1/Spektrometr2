/***************************************************************************
 *
 *     File: simpleGui.cpp
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
 *          are in the camera.cpp module.  Camera PixeLINK enumeration functions, are
 *          in this module.
 */


#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <vector>

#include "camera.h"
#include "cameraSelectCombo.h"
#include "previewButtons.h"

using namespace std;

//
// Useful defines and enums.
//
#define ASSERT(x)	do { assert((x)); } while(0)

// Prototypes
extern "C" void StopButtonPressed
  (GtkWidget* widget, GdkEventExpose* event, gpointer userdata);

// The currently selected camera.  a NULL value indicates not camera has been selected
PxLCamera  *pCamera = NULL;

// Our GUI control objects.
GtkWindow *topLevelWindow;
PxLCameraSelectCombo *csObject = NULL;
PxLPreviewButtons *previewObject = NULL;


int main(int argc, char* argv[]) {

    GtkBuilder *builder;
    GtkWidget  *window;
    GError     *error = NULL;

    gtk_init( &argc, &argv );

    builder = gtk_builder_new();
    // Load UI from file. If error occurs, report it and quit application.
    if( ! gtk_builder_add_from_file( builder, "simpleGui.glade", &error ) )
    {
        g_warning( "%s", error->message );
        //g_free( error );
        return( 1 );
    }

    // Get the main window pointer from the UI
    window = GTK_WIDGET( gtk_builder_get_object( builder, "windowMain" ) );
    topLevelWindow = GTK_WINDOW (window);

    // Create our objects.  They do all of the real work.
    csObject = new PxLCameraSelectCombo (GTK_WIDGET( gtk_builder_get_object( builder, "CameraSelect_ComboBoxText" ) ) );
    previewObject = new PxLPreviewButtons
    						( GTK_WIDGET( gtk_builder_get_object( builder, "Play_Button" ) ),
    						  GTK_WIDGET( gtk_builder_get_object( builder, "Pause_Button" ) ),
    						  GTK_WIDGET( gtk_builder_get_object( builder, "Stop_Button" ) ) );

    gtk_builder_connect_signals( builder, NULL );
    // We don't need the builder anymore, so destroy it.
    g_object_unref( G_OBJECT( builder ) );

    // Show the window
    gtk_widget_show( window );
    // and off we go....
    gtk_main();

    delete csObject;
    delete previewObject;

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
