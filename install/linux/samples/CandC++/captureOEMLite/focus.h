/*
 * focus.h
 *
 *  Created on: Apr 27, 2017
 *      Author: pcarroll
 */

#if !defined(PIXELINK_FOCUS_H)
#define PIXELINK_FOCUS_H

#include "PixeLINKApi.h"
#include "slider.h"
#include <gtk/gtk.h>

class PxLFocus
{
public:
    // Constructor
    PxLFocus (GtkWidget *min, GtkWidget *max, GtkWidget *scale,
             GtkWidget *value, GtkWidget *oneTime, GtkWidget *continous);
    // Destructor
    ~PxLFocus ();

    void  greyAll ();
    void  initialize ();

    void  activateAutos ();

    PxLSlider    *m_slider;
    GtkWidget    *m_oneTime;
    GtkWidget    *m_continous;

    float         m_lastReadValue;
};

#endif // !defined(PIXELINK_FOCUS_H)
