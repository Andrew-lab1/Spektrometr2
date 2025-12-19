
/***************************************************************************
 *
 *     File: gain.h
 *
 *     Description: Simple wrapper class for all of the gain controls
 *
 */

#if !defined(PIXELINK_GAIN_H)
#define PIXELINK_GAIN_H

#include "PixeLINKApi.h"
#include "slider.h"
#include <gtk/gtk.h>

class PxLGain
{
public:
    // Constructor
	PxLGain (GtkWidget *min, GtkWidget *max, GtkWidget *scale,
	         GtkWidget *value, GtkWidget *oneTime, GtkWidget *continous);
	// Destructor
	~PxLGain ();

    void  greyAll ();
    void  initialize ();

    void  activateAutos ();

    PxLSlider    *m_slider;
    GtkWidget    *m_oneTime;
    GtkWidget    *m_continous;

    float         m_lastReadValue;
};

#endif // !defined(PIXELINK_GAIN_H)
