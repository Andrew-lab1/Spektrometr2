
/***************************************************************************
 *
 *     File: whiteBalance.cpp
 *
 *     Description: All white balance controls.
 *
 *     Design Notes:
 *       - This module assumes that the cameras uses the same 'range' for each of the color channels.
 *         That is, min(red) == min(blue) == min(green), and max(red) == max(green) == max(blue)
 *       - This module uses only one signal handler for all of the color channel edit fields, and another
 *         signal handler for all of the scale handlers for the color channels.  Consequently, when either
 *         the edit field, or or the scale field changes for one of the color channels, then the
 *         edit field of the other color channels is also read.
 */

#include "captureOEMLite.h"


PxLWhiteBalance::PxLWhiteBalance (GtkWidget *rMin, GtkWidget *rMax, GtkWidget *rScale, GtkWidget *rValue,
                                  GtkWidget *gMin, GtkWidget *gMax, GtkWidget *gScale, GtkWidget *gValue,
                                  GtkWidget *bMin, GtkWidget *bMax, GtkWidget *bScale, GtkWidget *bValue,
                                  GtkWidget *oneTime)
: m_oneTime(oneTime)
{
    m_rSlider = new PxLSlider (rMin, rMax, rScale, rValue);
    m_gSlider = new PxLSlider (gMin, gMax, gScale, gValue);
    m_bSlider = new PxLSlider (bMin, bMax, bScale, bValue);
}

PxLWhiteBalance::~PxLWhiteBalance ()
{
    delete m_rSlider;
    delete m_gSlider;
    delete m_bSlider;
}

// Disables all white balance controls/
void PxLWhiteBalance:: greyAll()
{
	m_rSlider->greyAll();
    m_gSlider->greyAll();
    m_bSlider->greyAll();
    gtk_widget_set_sensitive (m_oneTime, false);
}

// enables all supported white balance controls, and initializes them to current values.
void PxLWhiteBalance:: initialize()
{
    PxLAutoLock lock(&pCameraLock);

    if (pCamera)
    {
        if (pCamera->supported(FEATURE_WHITE_SHADING))
        {
            float min, max;
            float red, green, blue;

            m_rSlider->activate(true);
            m_gSlider->activate(true);
            m_bSlider->activate(true);
            pCamera->getWhiteBalanceRange(&min, &max);
            m_rSlider->setRange(min, max);
            m_gSlider->setRange(min, max);
            m_bSlider->setRange(min, max);
            pCamera->getWhiteBalanceValues(&red, &green, &blue);
            m_rSlider->setValue(red);
            m_gSlider->setValue(green);
            m_bSlider->setValue(blue);

            activateAutos();
        }
    }
}

void PxLWhiteBalance:: activateAutos()
{
    bool oneTimeEnable = false;

    PxLAutoLock lock(&pCameraLock);
    if (pCamera)
    {
        if (pCamera->supported(FEATURE_WHITE_SHADING))
        {
            // Only enable the oneTime auto feature if we are streaming
            if (pCamera->streaming() && pCamera->oneTimeSuppored(FEATURE_WHITE_SHADING)) oneTimeEnable = true;
        }
    }

    gtk_widget_set_sensitive (m_oneTime, oneTimeEnable);
}

extern "C" void WhiteBalanceValueChanged
    (GtkWidget* widget, GdkEventExpose* event, gpointer userdata )
{
    if (!pCamera) return;

    float newRed, newGreen, newBlue;

    newRed   = whiteBalanceObject->m_rSlider->getEditValue();
    newGreen = whiteBalanceObject->m_gSlider->getEditValue();
    newBlue  = whiteBalanceObject->m_bSlider->getEditValue();

    pCamera->setWhiteBalanceValues(newRed, newGreen, newBlue);

    // read it back again to see if the camera accepted it, or perhaps 'rounded it' to a
    // new value
    pCamera->getWhiteBalanceValues(&newRed, &newGreen, &newBlue);

    whiteBalanceObject->m_rSlider->setValue(newRed);
    whiteBalanceObject->m_gSlider->setValue(newGreen);
    whiteBalanceObject->m_bSlider->setValue(newBlue);
}

extern "C" void WhiteBalanceScaleChanged
    (GtkWidget* widget, GdkEventExpose* event, gpointer userdata )
{
    if (!pCamera) return;

    // we are only interested in changes to the scale from user input
    if (whiteBalanceObject->m_rSlider->rangeChangeInProgress() ||
        whiteBalanceObject->m_gSlider->rangeChangeInProgress() ||
        whiteBalanceObject->m_bSlider->rangeChangeInProgress()) return;
    if (whiteBalanceObject->m_rSlider->setIsInProgress() ||
        whiteBalanceObject->m_gSlider->setIsInProgress() ||
        whiteBalanceObject->m_rSlider->setIsInProgress()) return;

    float newRed, newGreen, newBlue;

    newRed   = whiteBalanceObject->m_rSlider->getScaleValue();
    newGreen = whiteBalanceObject->m_gSlider->getScaleValue();
    newBlue  = whiteBalanceObject->m_bSlider->getScaleValue();

    pCamera->setWhiteBalanceValues(newRed, newGreen, newBlue);
    // read it back again to see if the camera accepted it, or perhaps 'rounded it' to a
    // new value
    pCamera->getWhiteBalanceValues(&newRed, &newGreen, &newBlue);

    whiteBalanceObject->m_rSlider->setValue(newRed);
    whiteBalanceObject->m_gSlider->setValue(newGreen);
    whiteBalanceObject->m_bSlider->setValue(newBlue);
}

extern "C" void WhiteBalanceOneTimeButtonPressed
    (GtkWidget* widget, GdkEventExpose* event, gpointer userdata )
{
    if (!pCamera) return;
    if (!pCamera->oneTimeSuppored(FEATURE_WHITE_SHADING)) return;

    pCamera->performOneTimeAuto(FEATURE_WHITE_SHADING);

    float newRed, newGreen, newBlue;
    PXL_RETURN_CODE rc = pCamera->getWhiteBalanceValues(&newRed, &newGreen, &newBlue);

    if (!API_SUCCESS(rc)) return;

    whiteBalanceObject->m_rSlider->setValue(newRed);
    whiteBalanceObject->m_gSlider->setValue(newGreen);
    whiteBalanceObject->m_bSlider->setValue(newBlue);
}

