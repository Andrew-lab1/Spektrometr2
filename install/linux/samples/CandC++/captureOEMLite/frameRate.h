
/***************************************************************************
 *
 *     File: frameRate.h
 *
 *     Description: Simple wrapper class for all of the frame rate controls
 *
 */

#if !defined(PIXELINK_FRAME_RATE_H)
#define PIXELINK_FRAME_RATE_H

#include "PixeLINKApi.h"
#include "slider.h"
#include <gtk/gtk.h>

class PxLFrameRate
{
public:
    // Constructor
	PxLFrameRate (GtkWidget *min, GtkWidget *max, GtkWidget *scale,
	              GtkWidget *value, GtkWidget *oneTime, GtkWidget *continous);
	// Destructor
	~PxLFrameRate ();

    void  greyAll ();
    void  initialize ();

    void  activateAutos ();
    void  updateFrameRate ();

    PxLSlider    *m_slider;
    GtkWidget    *m_oneTime;
    GtkWidget    *m_continous;

};

#endif // !defined(PIXELINK_FRAME_RATE_H)
