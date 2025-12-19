
/***************************************************************************
 *
 *     File: imageFlip.cpp
 *
 *     Description: All image flip controls.
 *
 */

#include "captureOEMLite.h"


PxLImageFlip::PxLImageFlip (GtkWidget *horizontalCheck, GtkWidget *verticalCheck)
: m_horizontalCheck(horizontalCheck)
, m_verticalCheck(verticalCheck)
{
}

// Disables all white balance controls/
void PxLImageFlip:: greyAll()
{
    gtk_widget_set_sensitive (m_horizontalCheck, false);
    gtk_widget_set_sensitive (m_verticalCheck, false);
}

// enables all supported image flip controls, and initializes them to current values.
void PxLImageFlip:: initialize()
{
    PxLAutoLock lock(&pCameraLock);

    if (pCamera)
    {
        if (pCamera->supported(FEATURE_FLIP))
        {
            gtk_widget_set_sensitive (m_horizontalCheck, true);
            gtk_widget_set_sensitive (m_verticalCheck, true);

            bool horizontal = false;
            bool vertical = false;

            pCamera->getFlip(&horizontal, &vertical);

            gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(m_horizontalCheck), horizontal);
            gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(m_verticalCheck), vertical);
        }
    }
}

extern "C" void ImageFlipToggled
    (GtkWidget* widget, GdkEventExpose* event, gpointer userdata )
{
    if (!pCamera) return;

    bool horizontalOn = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON(imageFlipObject->m_horizontalCheck));
    bool verticalOn   = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON(imageFlipObject->m_verticalCheck));

    pCamera->setFlip(horizontalOn, verticalOn);
}
