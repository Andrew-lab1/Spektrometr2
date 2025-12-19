
/***************************************************************************
 *
 *     File: expsoure.h
 *
 *     Description: Simple wrapper class for all of the exposure controls
 *
 */

#if !defined(PIXELINK_EXPOSURE_H)
#define PIXELINK_EXPOSURE_H

#include "PixeLINKApi.h"
#include "slider.h"
#include <gtk/gtk.h>

class PxLExposure
{
public:
    // Constructor
	PxLExposure (GtkWidget *min, GtkWidget *max, GtkWidget *scale,
	             GtkWidget *value, GtkWidget *oneTime, GtkWidget *continous);
	// Destructor
	~PxLExposure ();

    void  greyAll ();
    void  initialize ();

    void  activateAutos ();

    PxLSlider    *m_slider;
    GtkWidget    *m_oneTime;
    GtkWidget    *m_continous;

    float         m_lastReadValue; // in millisecond units

};

#endif // !defined(PIXELINK_EXPOSURE_H)
