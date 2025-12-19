
/***************************************************************************
 *
 *     File: frameRate.cpp
 *
 *     Description: All frame rate controls.
 *
 *     Design Notes:
 *       - Note that we treat the 'continuous auto' of this feature differently than we do for
 *         other features.  Other features use the feature poller thread to 'monitor' feature values
 *         that the camera might autonomously change when in continuous auto mode.  Things like exposure
 *         or white balance are adjusted depending on light levels and the target being photographed.
 *         However,    Frame rate is not like that -- if the camera is free to adjust it (in continuous
 *         auto mode), it will only make changes to it when some other feature changes as a result of
 *         user input.
 *
 *         So, frame rate does not 'bother' with the poller, as it does not make sense to keep polling
 *         something that isn't going to change.  Rather, this class will export an update method that
 *         other classes can use, whenever they make an adjustment to something that might cause a change
 *         in frame rate.
 */

#include "captureOEMLite.h"

// Prototypes
PXL_RETURN_CODE getCurrentFrameRate();
void updateFrameRateControls();

PxLFrameRate::PxLFrameRate (GtkWidget *min, GtkWidget *max, GtkWidget *scale,
                            GtkWidget *value, GtkWidget *oneTime, GtkWidget *continous)
: m_oneTime(oneTime)
, m_continous(continous)
{
    m_slider = new PxLSlider (min, max, scale, value);
}

PxLFrameRate::~PxLFrameRate ()
{
    delete m_slider;
}

// Disables all frame rate controls/
void PxLFrameRate:: greyAll()
{
	m_slider->greyAll();
    gtk_widget_set_sensitive (m_oneTime, false);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(m_continous), false);
	gtk_widget_set_sensitive (m_continous, false);
}

// enables all supported frame controls, and initializes them to current values.
void PxLFrameRate:: initialize()
{
    PxLAutoLock lock(&pCameraLock);

    if (pCamera)
    {
        if (pCamera->supported(FEATURE_FRAME_RATE))
        {
            float min, max, value;

            m_slider->activate(true);
            pCamera->getRange(FEATURE_FRAME_RATE, &min, &max);
            m_slider->setRange(min, max);
            pCamera->getValue(FEATURE_FRAME_RATE, &value);
            m_slider->setValue(value);

            activateAutos();
        }
    }
}

void PxLFrameRate:: activateAutos()
{
    bool oneTimeEnable = false;
    bool continuousEnable = false;
    bool continuousCurrentlyOn = false;

    PxLAutoLock lock(&pCameraLock);
    if (pCamera)
    {
        if (pCamera->supported(FEATURE_FRAME_RATE))
        {
            if (pCamera->oneTimeSuppored(FEATURE_FRAME_RATE)) oneTimeEnable = true;
            if (pCamera->continuousSupported(FEATURE_FRAME_RATE))
            {
                continuousEnable = true;
                pCamera->getContinuousAuto(FEATURE_FRAME_RATE, &continuousCurrentlyOn);
            }
        }
    }

    gtk_widget_set_sensitive (m_oneTime, oneTimeEnable && !continuousCurrentlyOn);
    gtk_widget_set_sensitive (m_continous, continuousEnable);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(m_continous), continuousCurrentlyOn);
    m_slider->activate (! continuousCurrentlyOn);
}

// Note that this method will update user controls, so it should only be called from a glade
// user thread (not one of our own threads).
void PxLFrameRate:: updateFrameRate()
{
    PXL_RETURN_CODE rc = ApiSuccess;

    PxLAutoLock lock(&pCameraLock);
    if (! pCamera || ! csObject ) return;
    if (csObject->changingCameras()) return;

    float current, min, max;
    rc = pCamera->getRange(FEATURE_FRAME_RATE, &min, &max);
    if (API_SUCCESS(rc))
    {
        // IMPORTANT NOTE:
        //     Note how we are using FEATURE_ACTUAL_FRAME_RATE, not FEATURE_FRAME_RATE.  The former is
        //     what the camera is actually using, and the latter is what the user wants the frame rate
        //     to be.  They may differ if the user wants some value, but that is not achievable due to
        //     say, a log exposure setting, or a bandwidth limitation.
        //
        //     However, this routine is called for the former instances, when the camera is choosing
        //     the frame rate, and may have adjusted it do to a change of some other parameter.
        rc = pCamera->getValue(FEATURE_ACTUAL_FRAME_RATE, &current);
    }
    if (!API_SUCCESS(rc)) return;

    m_slider->setRange(min, max);
    m_slider->setValue(current);
}

extern "C" void FrameRateValueChanged
    (GtkWidget* widget, GdkEventExpose* event, gpointer userdata )
{
    if (! pCamera || ! csObject ) return;
    if (csObject->changingCameras()) return;

    float newFrameRate;

    newFrameRate = frameRateObject->m_slider->getEditValue();

    // Bugzilla.995 - Only set the frame rate if it's a new user value
    float oldFrameRate;
    pCamera->getValue(FEATURE_FRAME_RATE, &oldFrameRate);
    if (oldFrameRate == newFrameRate) return;

    pCamera->setValue(FEATURE_FRAME_RATE, newFrameRate);

    // read it back again to see if the camera accepted it, or perhaps 'rounded it' to a
    // new value
    pCamera->getValue(FEATURE_FRAME_RATE, &newFrameRate);
    frameRateObject->m_slider->setValue(newFrameRate);
}

extern "C" void FrameRateScaleChanged
    (GtkWidget* widget, GdkEventExpose* event, gpointer userdata )
{
    if (! pCamera || ! csObject ) return;
    if (csObject->changingCameras()) return;

    // we are only interested in changes to the scale from user input
    if (frameRateObject->m_slider->rangeChangeInProgress()) return;
    if (frameRateObject->m_slider->setIsInProgress()) return;

    float newFrameRate;

    newFrameRate = frameRateObject->m_slider->getScaleValue();

    pCamera->setValue(FEATURE_FRAME_RATE, newFrameRate);
    // read it back again to see if the camera accepted it, or perhaps 'rounded it' to a
    // new value
    pCamera->getValue(FEATURE_FRAME_RATE, &newFrameRate);
    frameRateObject->m_slider->setValue(newFrameRate);
}

extern "C" void FrameRateOneTimeButtonPressed
    (GtkWidget* widget, GdkEventExpose* event, gpointer userdata )
{
    if (! pCamera || ! csObject ) return;
    if (!pCamera->oneTimeSuppored(FEATURE_FRAME_RATE)) return;
    if (csObject->changingCameras()) return;

    pCamera->performOneTimeAuto(FEATURE_FRAME_RATE);

    float newFrameRate;
    PXL_RETURN_CODE rc = pCamera->getValue(FEATURE_FRAME_RATE, &newFrameRate);

    if (!API_SUCCESS(rc)) return;

    frameRateObject->m_slider->setValue(newFrameRate);
}

extern "C" void FrameRateContinuousToggled
    (GtkWidget* widget, GdkEventExpose* event, gpointer userdata )
{
    PXL_RETURN_CODE rc;

    if (! pCamera || ! csObject ) return;
    if (!pCamera->continuousSupported(FEATURE_FRAME_RATE)) return;
    if (csObject->changingCameras()) return;

    bool continuousOn = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON(frameRateObject->m_continous));

    rc = pCamera->setContinuousAuto(FEATURE_FRAME_RATE, continuousOn);
    if (!API_SUCCESS(rc)) return;

    // ensure the Make the slider and the oneTime buttons are only writable if we are not continuously adjusting
    frameRateObject->m_slider->activate (!continuousOn);
    // Also, oneTime should only ever be if while streaming. (and is supported, bugzilla.876)
    if (pCamera->oneTimeSuppored(FEATURE_FRAME_RATE))
    {
        gtk_widget_set_sensitive (frameRateObject->m_oneTime, pCamera->streaming() && !continuousOn);
    }

    // As per the design notes at the top of this module, We don't bother with a poll functions.  However,
    // we should read it now to see what the camera has choosen
    frameRateObject->updateFrameRate();
}

