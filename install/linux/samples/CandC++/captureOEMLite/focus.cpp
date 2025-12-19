/*
 * focus.cpp
 *
 *  Created on: Apr 27, 2017
 *      Author: pcarroll
 */

#include "captureOEMLite.h"


PxLFocus::PxLFocus (GtkWidget *min, GtkWidget *max, GtkWidget *scale,
                    GtkWidget *value, GtkWidget *oneTime, GtkWidget *continous)
: m_oneTime(oneTime)
, m_continous(continous)
, m_lastReadValue(0.0)
{
    m_slider = new PxLSlider (min, max, scale, value);
}

PxLFocus::~PxLFocus ()
{
    delete m_slider;
}

// Disables all focus controls/
void PxLFocus:: greyAll()
{
    m_slider->greyAll();
    gtk_widget_set_sensitive (m_oneTime, false);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(m_continous), false);
    gtk_widget_set_sensitive (m_continous, false);
}

// enables all supported focus controls, and initializes them to current values.
void PxLFocus:: initialize()
{
    PxLAutoLock lock(&pCameraLock);

    if (pCamera)
    {
        if (pCamera->supported(FEATURE_FOCUS))
        {
            float min, max, value;

            m_slider->activate(true);
            pCamera->getRange(FEATURE_FOCUS, &min, &max);
            m_slider->setRange(min, max);
            pCamera->getValue(FEATURE_FOCUS, &value);
            m_slider->setValue(value);

            activateAutos();
        }
    }
}

void PxLFocus:: activateAutos()
{
    bool oneTimeEnable = false;
    bool continuousEnable = false;
    bool continuousCurrentlyOn = false;

    PxLAutoLock lock(&pCameraLock);
    if (pCamera)
    {
        if (pCamera->supported(FEATURE_FOCUS))
        {
            // Only enable the oneTime auto feature if we are streaming
            if (pCamera->streaming() && pCamera->oneTimeSuppored(FEATURE_FOCUS)) oneTimeEnable = true;
            if (pCamera->continuousSupported(FEATURE_FOCUS))
            {
                continuousEnable = true;
                pCamera->getContinuousAuto(FEATURE_FOCUS, &continuousCurrentlyOn);
            }
        }
    }

    gtk_widget_set_sensitive (m_oneTime, oneTimeEnable && !continuousCurrentlyOn);
    gtk_widget_set_sensitive (m_continous, continuousEnable);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(m_continous), continuousCurrentlyOn);
    m_slider->activate (! continuousCurrentlyOn);
}

//
// Called periodically when doing continuous focus updates -- reads the current value
PXL_RETURN_CODE getCurrentFocus()
{
    PXL_RETURN_CODE rc = ApiSuccess;

    PxLAutoLock lock(&pCameraLock);
    if (pCamera)
    {
        // It's safe to assume the camera supports focus, as this function will not be called
        // otherwise.  If we were to check via pCamera->supported (FEATURE_FOCUS) or
        // pCamera->continuousSupported (FEATURE_FOCUS), then that will perform a PxLGetCameraFeatures,
        // which is a lot of work for not.
        float focus = 0.0;
        rc = pCamera->getValue(FEATURE_FOCUS, &focus);
        if (API_SUCCESS(rc)) focusObject->m_lastReadValue = focus;
    }

    return rc;
}

//
// Called periodically when doing continuous focus updates -- updates the user controls
void updateFocusControls()
{
    if (pCamera)
    {
        focusObject->m_slider->setValue(focusObject->m_lastReadValue);
    }
}

extern "C" void FocusValueChanged
    (GtkWidget* widget, GdkEventExpose* event, gpointer userdata )
{
    if (!pCamera) return;

    float newFocus;

    newFocus = focusObject->m_slider->getEditValue();

    pCamera->setValue(FEATURE_FOCUS, newFocus);

    // read it back again to see if the camera accepted it, or perhaps 'rounded it' to a
    // new value
    pCamera->getValue(FEATURE_FOCUS, &newFocus);
    focusObject->m_slider->setValue(newFocus);

}

extern "C" void FocusScaleChanged
    (GtkWidget* widget, GdkEventExpose* event, gpointer userdata )
{
    if (!pCamera) return;

    // we are only interested in changes to the scale from user input
    if (focusObject->m_slider->rangeChangeInProgress()) return;
    if (focusObject->m_slider->setIsInProgress()) return;

    float newFocus;

    newFocus = focusObject->m_slider->getScaleValue();

    pCamera->setValue(FEATURE_FOCUS, newFocus);
    // read it back again to see if the camera accepted it, or perhaps 'rounded it' to a
    // new value
    pCamera->getValue(FEATURE_FOCUS, &newFocus);
    focusObject->m_slider->setValue(newFocus);

}

extern "C" void FocusOneTimeButtonPressed
    (GtkWidget* widget, GdkEventExpose* event, gpointer userdata )
{
    if (!pCamera) return;
    if (!pCamera->oneTimeSuppored(FEATURE_FOCUS)) return;

    pCamera->performOneTimeAuto(FEATURE_FOCUS);

    float newFocus;
    PXL_RETURN_CODE rc = pCamera->getValue(FEATURE_FOCUS, &newFocus);

    if (API_SUCCESS(rc))
    {
        focusObject->m_slider->setValue(newFocus);
    }

}

extern "C" void FocusContinuousToggled
    (GtkWidget* widget, GdkEventExpose* event, gpointer userdata )
{
    PXL_RETURN_CODE rc;

    if (!pCamera) return;
    if (!pCamera->continuousSupported(FEATURE_FOCUS)) return;

    bool continuousOn = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON(focusObject->m_continous));

    rc = pCamera->setContinuousAuto(FEATURE_FOCUS, continuousOn);
    if (!API_SUCCESS(rc)) return;

    // ensure the Make the slider and the oneTime buttons are only writable if we are not continuously adjusting
    focusObject->m_slider->activate (!continuousOn);
    // Also, onTime should only ever be if while streaming.
    gtk_widget_set_sensitive (focusObject->m_oneTime, pCamera->streaming() && !continuousOn);

    const PxLFeaturePollFunctions myFuncs (getCurrentFocus, updateFocusControls);
    if (continuousOn)
    {
        // add our functions to the continuous poller
        pCamera->m_poller->pollAdd(myFuncs);
    } else {
        // remove our functions from the continuous poller
        pCamera->m_poller->pollRemove(myFuncs);
    }
}




