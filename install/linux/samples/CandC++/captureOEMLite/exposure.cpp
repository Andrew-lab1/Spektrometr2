
/***************************************************************************
 *
 *     File: exposure.cpp
 *
 *     Description: All exposure controls.
 */

#include "captureOEMLite.h"

// Local prototypes
PXL_RETURN_CODE getCurrentExposure();
void updateExposureControls();
const PxLFeaturePollFunctions myFuncs (getCurrentExposure, updateExposureControls);


PxLExposure::PxLExposure (GtkWidget *min, GtkWidget *max, GtkWidget *scale,
                          GtkWidget *value, GtkWidget *oneTime, GtkWidget *continous)
: m_oneTime(oneTime)
, m_continous(continous)
, m_lastReadValue(0.0)
{
    m_slider = new PxLSlider (min, max, scale, value);
}

PxLExposure::~PxLExposure ()
{
    delete m_slider;
}

// Disables all exposure controls/
void PxLExposure:: greyAll()
{
	m_slider->greyAll();
    gtk_widget_set_sensitive (m_oneTime, false);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(m_continous), false);
	gtk_widget_set_sensitive (m_continous, false);
}

// enables all supported exposure controls, and initializes them to current values.
void PxLExposure:: initialize()
{
    PxLAutoLock lock(&pCameraLock);

    if (pCamera)
    {
        if (pCamera->supported(FEATURE_EXPOSURE))
        {
            float min, max, value;

            // Note that FEATURE_EXPOSURE deals with units of seconds, while our controls
            // use units of milliseconds, so we need to convert from one to the other.
            m_slider->activate(true);
            pCamera->getRange(FEATURE_EXPOSURE, &min, &max);
            m_slider->setRange(min*1000, max*1000);
            pCamera->getValue(FEATURE_EXPOSURE, &value);
            m_slider->setValue(value*1000);

            activateAutos();
        }
    }
}

void PxLExposure::activateAutos()
{
    bool oneTimeEnable = false;
    bool continuousEnable = false;
    bool continuousCurrentlyOn = false;

    PxLAutoLock lock(&pCameraLock);
    if (pCamera)
    {
        if (pCamera->supported(FEATURE_EXPOSURE))
        {
            // Only enable the oneTime auto feature if we are streaming
            if (pCamera->streaming() && pCamera->oneTimeSuppored(FEATURE_EXPOSURE)) oneTimeEnable = true;
            if (pCamera->continuousSupported(FEATURE_EXPOSURE))
            {
                continuousEnable = true;
                pCamera->getContinuousAuto(FEATURE_EXPOSURE, &continuousCurrentlyOn);
            }
        }
    }

    gtk_widget_set_sensitive (m_oneTime, oneTimeEnable && !continuousCurrentlyOn);
    gtk_widget_set_sensitive (m_continous, continuousEnable);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(m_continous), continuousCurrentlyOn);
    m_slider->activate (! continuousCurrentlyOn);

    // Bugzilla.949 - Leave our poller running if we support either auto
    if (continuousEnable || oneTimeEnable)
    {
        // add our functions to the continuous poller
        pCamera->m_poller->pollAdd(myFuncs);
    }
}

//
// Called periodically when doing continuous exposure updates -- reads the current value
PXL_RETURN_CODE getCurrentExposure()
{
    PXL_RETURN_CODE rc = ApiSuccess;

    PxLAutoLock lock(&pCameraLock);
    if (pCamera)
    {
        // It's safe to assume the camera supports exposure, as this function will not be called
        // otherwise.  If we were to check via pCamera->supported (FEATURE_EXPOSURE) or
        // pCamera->continuousSupported (FEATURE_EXPSOURE), then that will perform a PxLGetCameraFeatures,
        // which is a lot of work for not.
        float exposureinSeconds = 0.0;
        rc = pCamera->getValue(FEATURE_EXPOSURE, &exposureinSeconds);
        if (API_SUCCESS(rc)) exposureObject->m_lastReadValue = exposureinSeconds * 1000;
    }

    return rc;
}

//
// Called periodically when doing continuous exposure updates -- updates the user controls
void updateExposureControls()
{
    if (pCamera)
    {
        exposureObject->m_slider->setValue(exposureObject->m_lastReadValue);
    }
}

extern "C" void ExposureValueChanged
    (GtkWidget* widget, GdkEventExpose* event, gpointer userdata )
{
    if (! pCamera || ! csObject ) return;
    if (csObject->changingCameras()) return;

    float newExposure;

    newExposure = exposureObject->m_slider->getEditValue();

    // NOTE:
    //     FEATURE_EXPOSURE deals with units of seconds, while our controls
    //     use units of milliseconds, so we need to convert from one to the other.
    pCamera->setValue(FEATURE_EXPOSURE, newExposure/1000);

    // read it back again to see if the camera accepted it, or perhaps 'rounded it' to a
    // new value
    pCamera->getValue(FEATURE_EXPOSURE, &newExposure);
    exposureObject->m_slider->setValue(newExposure*1000);

    // Update our frame rate control, as it's limit have probably changed.
    frameRateObject->updateFrameRate();
}

extern "C" void ExposureScaleChanged
    (GtkWidget* widget, GdkEventExpose* event, gpointer userdata )
{

    if (! pCamera || ! csObject ) return;
    if (csObject->changingCameras()) return;

    // we are only interested in changes to the scale from user input
    if (exposureObject->m_slider->rangeChangeInProgress()) return;
    if (exposureObject->m_slider->setIsInProgress()) return;

    float newExposure;

    newExposure = exposureObject->m_slider->getScaleValue();

    // NOTE:
    //     FEATURE_EXPOSURE deals with units of seconds, while our controls
    //     use units of milliseconds, so we need to convert from one to the other.
    pCamera->setValue(FEATURE_EXPOSURE, newExposure/1000);
    // read it back again to see if the camera accepted it, or perhaps 'rounded it' to a
    // new value
    pCamera->getValue(FEATURE_EXPOSURE, &newExposure);
    exposureObject->m_slider->setValue(newExposure*1000);

    // Update our frame rate control, as it's limit have probably changed.
    frameRateObject->updateFrameRate();
}

extern "C" void ExposureOneTimeButtonPressed
    (GtkWidget* widget, GdkEventExpose* event, gpointer userdata )
{
    if (! pCamera || ! csObject ) return;
    if (!pCamera->oneTimeSuppored(FEATURE_EXPOSURE)) return;
    if (csObject->changingCameras()) return;

    pCamera->performOneTimeAuto(FEATURE_EXPOSURE);

    float newExposure;
    PXL_RETURN_CODE rc = pCamera->getValue(FEATURE_EXPOSURE, &newExposure);

    if (!API_SUCCESS(rc)) return;

    exposureObject->m_slider->setValue(newExposure*1000);

    // Update our frame rate control, as it's limit have probably changed.
    frameRateObject->updateFrameRate();
}

extern "C" void ExposureContinuousToggled
    (GtkWidget* widget, GdkEventExpose* event, gpointer userdata )
{
    PXL_RETURN_CODE rc;

    if (! pCamera || ! csObject ) return;
    if (!pCamera->continuousSupported(FEATURE_EXPOSURE)) return;
    if (csObject->changingCameras()) return;

    bool continuousOn = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON(exposureObject->m_continous));

    rc = pCamera->setContinuousAuto(FEATURE_EXPOSURE, continuousOn);
    if (!API_SUCCESS(rc)) return;

    // ensure the Make the slider and the oneTime buttons are only writable if we are not continuously adjusting
    exposureObject->m_slider->activate (!continuousOn);
    // Also, onTime should only ever be if while streaming.
    gtk_widget_set_sensitive (exposureObject->m_oneTime, pCamera->streaming() && !continuousOn);

    // bugzilla.949 -- leave the expsoure poller running
    //if (continuousOn)
    //{
    //    // add our functions to the continuous poller
    //    pCamera->m_poller->pollAdd(myFuncs);
    //} else {
    //    // remove our functions from the continuous poller
    //    pCamera->m_poller->pollRemove(myFuncs);
    //}
}

