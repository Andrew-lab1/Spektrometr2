
/***************************************************************************
 *
 *     File: cameraSelectCombo.h
 *
 *     Description: Simple wrapper class for the camera select combo list
 *
 */

#if !defined(PIXELINK_PREVIEW_BUTTONS_H)
#define PIXELINK_PREVIEW_BUTTONS_H

#include "PixeLINKApi.h"
#include <gtk/gtk.h>

class PxLPreviewButtons
{
public:
    // Constructor
	PxLPreviewButtons (GtkWidget *play, GtkWidget *pause, GtkWidget *stop, GtkWidget *resize);
	// Destructor
	~PxLPreviewButtons ();

    void  greyAll ();
    void  activatePlay();
    void  activatePause();
    void  activateStop();

    GtkWidget    *m_play;
    GtkWidget    *m_pause;
    GtkWidget    *m_stop;
    GtkWidget    *m_resize;

};

#endif // !defined(PIXELINK_PREVIEW_BUTTONS_H)
