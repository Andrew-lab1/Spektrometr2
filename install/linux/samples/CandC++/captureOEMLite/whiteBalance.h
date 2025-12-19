
/***************************************************************************
 *
 *     File: whiteBalance.h
 *
 *     Description: Simple wrapper class for all of the whiteBalance controls
 *
 */

#if !defined(PIXELINK_WHITE_BALANCE_H)
#define PIXELINK_WHITE_BALANCE_H

#include "PixeLINKApi.h"
#include "slider.h"
#include <gtk/gtk.h>

class PxLWhiteBalance
{
public:
    // Constructor
	PxLWhiteBalance (GtkWidget *rMin, GtkWidget *rMax, GtkWidget *rScale, GtkWidget *rValue,
	                 GtkWidget *gMin, GtkWidget *gMax, GtkWidget *gScale, GtkWidget *gValue,
	                 GtkWidget *bMin, GtkWidget *bMax, GtkWidget *bScale, GtkWidget *bValue,
	                 GtkWidget *oneTime);
	// Destructor
	~PxLWhiteBalance ();

    void  greyAll ();
    void  initialize ();

    void  activateAutos ();

    PxLSlider    *m_rSlider;
    PxLSlider    *m_gSlider;
    PxLSlider    *m_bSlider;
    GtkWidget    *m_oneTime;
};

#endif // !defined(PIXELINK_WHITE_BALANCE_H)
