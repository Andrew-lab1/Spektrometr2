
/***************************************************************************
 *
 *     File: gain.cpp
 *
 *     Description: All gain controls.
 */

#include "captureOEMLite.h"


PxLGain::PxLGain (GtkWidget *min, GtkWidget *max, GtkWidget *scale,
                  GtkWidget *value, GtkWidget *oneTime, GtkWidget *continous)
: m_oneTime(oneTime)
, m_continous(continous)
, m_lastReadValue(0.0)
{
    m_slider = new PxLSlider (min, max, scale, value);
}

PxLGain::~PxLGain ()
{
    delete m_slider;
}

// Disables all gain controls/
void PxLGain:: greyAll()
{
	m_slider->greyAll();
    gtk_widget_set_sensitive (m_oneTime, false);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(m_continous), false);
	gtk_widget_set_sensitive (m_continous, false);
}

// enables all supported gain controls, and initializes them to current values.
void PxLGain:: initialize()
{
    PxLAutoLock lock(&pCameraLock);

    if (pCamera)
    {
        if (pCamera->supported(FEATURE_GAIN))
        {
            float min, max, value;

            m_slider->activate(true);
            pCamera->getRange(FEATURE_GAIN, &min, &max);
            m_slider->setRange(min, max);
            pCamera->getValue(FEATURE_GAIN, &value);
            m_slider->setValue(value);

            activateAutos();
        }
    }
}

void PxLGain:: activateAutos()
{
    bool oneTimeEnable = false;
    bool continuousEnable = false;
    bool continuousCurrentlyOn = false;

    PxLAutoLock lock(&pCameraLock);
    if (pCamera)
    {
        if (pCamera->supported(FEATURE_GAIN))
        {
            // Only enable the oneTime auto feature if we are streaming
            if (pCamera->streaming() && pCamera->oneTimeSuppored(FEATURE_GAIN)) oneTimeEnable = true;
            if (pCamera->continuousSupported(FEATURE_GAIN))
            {
                continuousEnable = true;
                pCamera->getContinuousAuto(FEATURE_GAIN, &continuousCurrentlyOn);
            }
        }
    }

    gtk_widget_set_sensitive (m_oneTime, oneTimeEnable && !continuousCurrentlyOn);
    gtk_widget_set_sensitive (m_continous, continuousEnable);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(m_continous), continuousCurrentlyOn);
    m_slider->activate (! continuousCurrentlyOn);
}

//
// Called periodically when doing continuous gain updates -- reads the current value
PXL_RETURN_CODE getCurrentGain()
{
    PXL_RETURN_CODE rc = ApiSuccess;

    PxLAutoLock lock(&pCameraLock);
    if (pCamera)
    {
        // It's safe to assume the camera supports gain, as this function will not be called
        // otherwise.  If we were to check via pCamera->supported (FEATURE_GAIN) or
        // pCamera->continuousSupported (FEATURE_GAIB), then that will perform a PxLGetCameraFeatures,
        // which is a lot of work for not.
        float gain = 0.0;
        rc = pCamera->getValue(FEATURE_GAIN, &gain);
        if (API_SUCCESS(rc)) gainObject->m_lastReadValue = gain;
    }

    return rc;
}

//
// Called periodically when doing continuous gain updates -- updates the user controls
void updateGainControls()
{
    if (pCamera)
    {
        gainObject->m_slider->setValue(gainObject->m_lastReadValue);
    }
}

extern "C" void GainValueChanged
    (GtkWidget* widget, GdkEventExpose* event, gpointer userdata )
{
    if (!pCamera) return;

    float newGain;

    newGain = gainObject->m_slider->getEditValue();

    pCamera->setValue(FEATURE_GAIN, newGain);

    // read it back again to see if the camera accepted it, or perhaps 'rounded it' to a
    // new value
    pCamera->getValue(FEATURE_GAIN, &newGain);
    gainObject->m_slider->setValue(newGain);

}

extern "C" void GainScaleChanged
    (GtkWidget* widget, GdkEventExpose* event, gpointer userdata )
{
    if (!pCamera) return;

    // we are only interested in changes to the scale from user input
    if (gainObject->m_slider->rangeChangeInProgress()) return;
    if (gainObject->m_slider->setIsInProgress()) return;

    float newGain;

    newGain = gainObject->m_slider->getScaleValue();

    pCamera->setValue(FEATURE_GAIN, newGain);
    // read it back again to see if the camera accepted it, or perhaps 'rounded it' to a
    // new value
    pCamera->getValue(FEATURE_GAIN, &newGain);
    gainObject->m_slider->setValue(newGain);

}

extern "C" void GainOneTimeButtonPressed
    (GtkWidget* widget, GdkEventExpose* event, gpointer userdata )
{
    if (!pCamera) return;
    if (!pCamera->oneTimeSuppored(FEATURE_GAIN)) return;

    pCamera->performOneTimeAuto(FEATURE_GAIN);

    float newGain;
    PXL_RETURN_CODE rc = pCamera->getValue(FEATURE_GAIN, &newGain);

    if (!API_SUCCESS(rc)) return;

    gainObject->m_slider->setValue(newGain);
}

extern "C" void GainContinuousToggled
    (GtkWidget* widget, GdkEventExpose* event, gpointer userdata )
{
    PXL_RETURN_CODE rc;

    if (!pCamera) return;
    if (!pCamera->continuousSupported(FEATURE_GAIN)) return;

    bool continuousOn = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON(gainObject->m_continous));

    rc = pCamera->setContinuousAuto(FEATURE_GAIN, continuousOn);
    if (!API_SUCCESS(rc)) return;

    // ensure the Make the slider and the oneTime buttons are only writable if we are not continuously adjusting
    gainObject->m_slider->activate (!continuousOn);
    // Also, onTime should only ever be if while streaming.
    gtk_widget_set_sensitive (gainObject->m_oneTime, pCamera->streaming() && !continuousOn);

    const PxLFeaturePollFunctions myFuncs (getCurrentGain, updateGainControls);
    if (continuousOn)
    {
        // add our functions to the continuous poller
        pCamera->m_poller->pollAdd(myFuncs);
    } else {
        // remove our functions from the continuous poller
        pCamera->m_poller->pollRemove(myFuncs);
    }
}

