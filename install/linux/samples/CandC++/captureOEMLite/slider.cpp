
/***************************************************************************
 *
 *     File: exposure.cpp
 *
 *     Description: All expsoure controls.
 */

#include "captureOEMLite.h"
#include <stdio.h>
#include <stdlib.h>


PxLSlider::PxLSlider (GtkWidget *min, GtkWidget *max, GtkWidget *scale, GtkWidget *value)
: m_min(min)
, m_max(max)
, m_scale(scale)
, m_value(value)
, m_rangeChangeInProgress(false)
{
}

PxLSlider::~PxLSlider ()
{
}

// All slider controls become non-selectable, and their values meaningless
void PxLSlider:: greyAll()
{
    gtk_range_set_range (GTK_RANGE (m_scale), 0, 0);
    gtk_label_set_text (GTK_LABEL (m_min), "");
    gtk_label_set_text (GTK_LABEL (m_max), "");
    gtk_entry_set_text (GTK_ENTRY (m_value), "");
    gtk_widget_set_sensitive (m_scale, false);
	gtk_widget_set_sensitive (m_value, false);
}

// Determines if the slider controls are selectable or not (and thus controls write access).
void PxLSlider:: activate(bool activated)
{
    gtk_widget_set_sensitive (m_scale, activated);
    gtk_widget_set_sensitive (m_value, activated);
}

// Sets new values for the sliders values (slider can be selectable, or not)
void PxLSlider:: setValue(double value)
{
    char cValue[40];

    m_setIsInProgress = true;

    gtk_range_set_value (GTK_RANGE (m_scale), value);

    // Bugzilla.877 - Don't bother updating the value edit box, if
    // it currently has the keyboard focus.
    if (! (gtk_widget_get_state_flags(m_value) & GTK_STATE_FLAG_FOCUSED))
    {
       if (value < 10)
       {
           // dealing with small values, so show 3 decimal places
           sprintf (cValue, "%5.3f",value);
           gtk_entry_set_text (GTK_ENTRY (m_value), cValue);
       } else if (value < 100) {
           // dealing with small-ish values, so show 1 decimal places
           sprintf (cValue, "%5.1f",value);
           gtk_entry_set_text (GTK_ENTRY (m_value), cValue);
       } else {
           // Large numbers -- Don't bother with the decimal point
           sprintf (cValue, "%5.0f",value);
           gtk_entry_set_text (GTK_ENTRY (m_value), cValue);
       }
    }
    m_setIsInProgress = false;
}

// Sets new values for the sliders values (slider can be selectable, or not)
void PxLSlider:: setRange(double min, double max)
{
    char cValue[40];
    int range = (int)max-min;

    m_rangeChangeInProgress = true;

    if (range < 0) return;  // invalid range

    if (range < 10)
    {
        // dealing with small values, so show 3 decimal places
        sprintf (cValue, "%5.3f",min);
        gtk_label_set_text (GTK_LABEL (m_min), cValue);
        sprintf (cValue, "%5.3f",max);
        gtk_label_set_text (GTK_LABEL (m_max), cValue);

        gtk_scale_set_digits (GTK_SCALE (m_scale), 3);
    } else if (range < 100){
        // dealing with small-ish values, so show 1 decimal places
        sprintf (cValue, "%5.1f",min);
        gtk_label_set_text (GTK_LABEL (m_min), cValue);
        sprintf (cValue, "%5.1f",max);
        gtk_label_set_text (GTK_LABEL (m_max), cValue);

        gtk_scale_set_digits (GTK_SCALE (m_scale), 1);
    } else {
        // Large numbers -- Don't bother with the decimal point
        sprintf (cValue, "%5.0f",min);
        gtk_label_set_text (GTK_LABEL (m_min), cValue);
        sprintf (cValue, "%5.0f",max);
        gtk_label_set_text (GTK_LABEL (m_max), cValue);

        gtk_scale_set_digits (GTK_SCALE (m_scale), 0);
    }

    gtk_range_set_range (GTK_RANGE (m_scale), min, max);

    m_rangeChangeInProgress = false;
}

// Returns the value currently in the 'value' edit box for the control
double PxLSlider:: getEditValue()
{
    return (atof (gtk_entry_get_text (GTK_ENTRY (m_value))));
}

// Returns the value currently specified by the 'scale' control (the actual slider)
double PxLSlider:: getScaleValue()
{
    return (gtk_range_get_value (GTK_RANGE (m_scale)));
}
