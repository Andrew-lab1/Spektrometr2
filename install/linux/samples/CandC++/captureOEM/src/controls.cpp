
/***************************************************************************
 *
 *     File: controls.cpp
 *
 *     Description:
 *        Controls for the 'Controls' tab  in CaptureOEM.
 */

#include <stdlib.h>
#include "controls.h"
#include "cameraSelect.h"
#include "stream.h"
#include "onetime.h"
#include "camera.h"
#include "captureOEM.h"
#if defined VIDEO_CAPTURE_SUPPORTED
#include "video.h"
#endif
#include "link.h"
#include "autoRoi.h"

using namespace std;

extern PxLControls     *gControlsTab;
extern PxLCameraSelect *gCameraSelectTab;
extern PxLStream       *gStreamTab;
extern PxLOnetime      *gOnetimeDialog;
#if defined VIDEO_CAPTURE_SUPPORTED
extern PxLVideo        *gVideoTab;
#endif
extern PxLLink         *gLinkTab;
extern PxLAutoRoi      *gAutoRoiTab;

//
// Local prototypes.
//    UI updates can only be done from a gtk thread -- these routines are gtk 'idle' threads
//    and as such are 'UI update safe'. For each 'feature', there there the following functions:
//       . {featureXXX}Deactivate - Makes the controls meaningless (including greying them out)
//       . {featreuXXX}Activate - updates the controls with values from the camera
static gboolean  RefreshComplete (gpointer pData);
static gboolean  ExposureDeactivate (gpointer pData);
static gboolean  ExposureActivate (gpointer pData);
static gboolean  FramerateDeactivate (gpointer pData);
static gboolean  FramerateActivate (gpointer pData);
static gboolean  GammaDeactivate (gpointer pData);
static gboolean  GammaActivate (gpointer pData);
static gboolean  GainDeactivate (gpointer pData);
static gboolean  GainActivate (gpointer pData);
static gboolean  HdrDeactivate (gpointer pData);
static gboolean  HdrActivate (gpointer pData);
static gboolean  ColortempDeactivate (gpointer pData);
static gboolean  ColortempActivate (gpointer pData);
static gboolean  SaturationDeactivate (gpointer pData);
static gboolean  SaturationActivate (gpointer pData);
static gboolean  BrightnessDeactivate (gpointer pData);
static gboolean  BrightnessActivate (gpointer pData);
static gboolean  WhitebalanceDeactivate (gpointer pData);
static gboolean  WhitebalanceActivate (gpointer pData);

// Prototypes for functions used for features with auto modes (continuous and onetime).
static PXL_RETURN_CODE GetCurrentExposure();
static void UpdateExposureControls();
static const PxLFeaturePollFunctions exposureFuncs (GetCurrentExposure, UpdateExposureControls);
static PXL_RETURN_CODE GetCurrentGain();
static void UpdateGainControls();
static const PxLFeaturePollFunctions gainFuncs (GetCurrentGain, UpdateGainControls);
static PXL_RETURN_CODE GetCurrentFramerate();
static void UpdateFramerateControls();
static const PxLFeaturePollFunctions framerateFuncs (GetCurrentFramerate, UpdateFramerateControls);
static PXL_RETURN_CODE GetCurrentWhitebalance();
static void UpdateWhitebalanceControls();
static const PxLFeaturePollFunctions whitebalanceFuncs (GetCurrentWhitebalance, UpdateWhitebalanceControls);

/* ---------------------------------------------------------------------------
 * --   Member functions - Public
 * ---------------------------------------------------------------------------
 */
PxLControls::PxLControls (GtkBuilder *builder)
: m_exposureLast (0)
, m_gainLast (0)
, m_framerateLast (0)
, m_framerateActualLast (0)
, m_redLast (0)
, m_greenLast (0)
, m_blueLast (0)
, m_cameraSupportsActualFramerate(false)
{

    // Exposure
    m_exposureLabel = GTK_WIDGET( gtk_builder_get_object( builder, "Exposure_Label" ) );
    m_exposureSlider = new PxLSlider (
            GTK_WIDGET( gtk_builder_get_object( builder, "ExposureMin_Label" ) ),
            GTK_WIDGET( gtk_builder_get_object( builder, "ExposureMax_Label" ) ),
            GTK_WIDGET( gtk_builder_get_object( builder, "Exposure_Scale" ) ),
            GTK_WIDGET( gtk_builder_get_object( builder, "Exposure_Text" ) ));
    m_exposureOneTime  = GTK_WIDGET( gtk_builder_get_object( builder, "ExposureOneTime_Button" ) );
    m_exposureContinous = GTK_WIDGET( gtk_builder_get_object( builder, "ExposureContinuous_Checkbutton" ) );
    m_exposureAutoMin = GTK_WIDGET( gtk_builder_get_object( builder, "ExposureAutoMin_Label" ) );
    m_exposureAutoMax = GTK_WIDGET( gtk_builder_get_object( builder, "ExposureAutoMax_Label" ) );
    m_exposureSetAutoMin = GTK_WIDGET( gtk_builder_get_object( builder, "ExposureAutoMin_Button" ) );
    m_exposureSetAutoMax = GTK_WIDGET( gtk_builder_get_object( builder, "ExposureAutoMax_Button" ) );

    // Framerate
    m_framerateSlider =  new PxLSlider (
        GTK_WIDGET( gtk_builder_get_object( builder, "FramerateMin_Label" ) ),
        GTK_WIDGET( gtk_builder_get_object( builder, "FramerateMax_Label" ) ),
        GTK_WIDGET( gtk_builder_get_object( builder, "Framerate_Scale" ) ),
        GTK_WIDGET( gtk_builder_get_object( builder, "Framerate_Text" ) ));
    m_framerateFixed  = GTK_WIDGET( gtk_builder_get_object( builder, "FramerateFixed_Checkbutton" ) );
    m_framerateContinous = GTK_WIDGET( gtk_builder_get_object( builder, "FramerateContinuous_Checkbutton" ) );
    m_framerateActual = GTK_WIDGET( gtk_builder_get_object( builder, "FramerateActual_Text" ) );

    // Gamma
    m_gammaSlider =  new PxLSlider (
        GTK_WIDGET( gtk_builder_get_object( builder, "GammaMin_Label" ) ),
        GTK_WIDGET( gtk_builder_get_object( builder, "GammaMax_Label" ) ),
        GTK_WIDGET( gtk_builder_get_object( builder, "Gamma_Scale" ) ),
        GTK_WIDGET( gtk_builder_get_object( builder, "Gamma_Text" ) ));
    m_gammaEnable  = GTK_WIDGET( gtk_builder_get_object( builder, "GammaEnable_Checkbutton" ) );

    // Gain
    m_gainSlider =  new PxLSlider (
        GTK_WIDGET( gtk_builder_get_object( builder, "GainMin_Label" ) ),
        GTK_WIDGET( gtk_builder_get_object( builder, "GainMax_Label" ) ),
        GTK_WIDGET( gtk_builder_get_object( builder, "Gain_Scale" ) ),
        GTK_WIDGET( gtk_builder_get_object( builder, "Gain_Text" ) ));
    m_gainOneTime  = GTK_WIDGET( gtk_builder_get_object( builder, "GainOneTime_Button" ) );
    m_gainContinous = GTK_WIDGET( gtk_builder_get_object( builder, "GainContinuous_Checkbutton" ) );

    // HDR (Gain based)
    m_hdrCombo = GTK_WIDGET( gtk_builder_get_object( builder, "Hdr_Combo" ) );

    // Colortemp
    m_colortempSlider =  new PxLSlider (
        GTK_WIDGET( gtk_builder_get_object( builder, "ColortempMin_Label" ) ),
        GTK_WIDGET( gtk_builder_get_object( builder, "ColortempMax_Label" ) ),
        GTK_WIDGET( gtk_builder_get_object( builder, "Colortemp_Scale" ) ),
        GTK_WIDGET( gtk_builder_get_object( builder, "Colortemp_Text" ) ));
    m_colortempEnable  = GTK_WIDGET( gtk_builder_get_object( builder, "ColortempEnable_Checkbutton" ) );

    // Saturation
    m_saturationSlider =  new PxLSlider (
        GTK_WIDGET( gtk_builder_get_object( builder, "SaturationMin_Label" ) ),
        GTK_WIDGET( gtk_builder_get_object( builder, "SaturationMax_Label" ) ),
        GTK_WIDGET( gtk_builder_get_object( builder, "Saturation_Scale" ) ),
        GTK_WIDGET( gtk_builder_get_object( builder, "Saturation_Text" ) ));

    // Brightness
    m_brightnessSlider =  new PxLSlider (
        GTK_WIDGET( gtk_builder_get_object( builder, "BrightnessMin_Label" ) ),
        GTK_WIDGET( gtk_builder_get_object( builder, "BrightnessMax_Label" ) ),
        GTK_WIDGET( gtk_builder_get_object( builder, "Brightness_Scale" ) ),
        GTK_WIDGET( gtk_builder_get_object( builder, "Brightness_Text" ) ));
        
    // Whitebalance
    m_redSlider =  new PxLSlider (
        GTK_WIDGET( gtk_builder_get_object( builder, "RedMin_Label" ) ),
        GTK_WIDGET( gtk_builder_get_object( builder, "RedMax_Label" ) ),
        GTK_WIDGET( gtk_builder_get_object( builder, "Red_Scale" ) ),
        GTK_WIDGET( gtk_builder_get_object( builder, "Red_Text" ) ));
    m_greenSlider =  new PxLSlider (
        GTK_WIDGET( gtk_builder_get_object( builder, "GreenMin_Label" ) ),
        GTK_WIDGET( gtk_builder_get_object( builder, "GreenMax_Label" ) ),
        GTK_WIDGET( gtk_builder_get_object( builder, "Green_Scale" ) ),
        GTK_WIDGET( gtk_builder_get_object( builder, "Green_Text" ) ));
    m_blueSlider =  new PxLSlider (
        GTK_WIDGET( gtk_builder_get_object( builder, "BlueMin_Label" ) ),
        GTK_WIDGET( gtk_builder_get_object( builder, "BlueMax_Label" ) ),
        GTK_WIDGET( gtk_builder_get_object( builder, "Blue_Scale" ) ),
        GTK_WIDGET( gtk_builder_get_object( builder, "Blue_Text" ) ));
    m_whitebalanceOneTime  = GTK_WIDGET( gtk_builder_get_object( builder, "WhitebalanceOneTime_Button" ) );
}

PxLControls::~PxLControls ()
{
}

void PxLControls::refreshRequired (bool noCamera)
{
    if (IsActiveTab (ControlsTab))
    {
        if (noCamera)
        {
            gdk_threads_add_idle ((GSourceFunc)ExposureDeactivate, this);
            gdk_threads_add_idle ((GSourceFunc)FramerateDeactivate, this);
            gdk_threads_add_idle ((GSourceFunc)GammaDeactivate, this);
            gdk_threads_add_idle ((GSourceFunc)GainDeactivate, this);
            gdk_threads_add_idle ((GSourceFunc)HdrDeactivate, this);
            gdk_threads_add_idle ((GSourceFunc)ColortempDeactivate, this);
            gdk_threads_add_idle ((GSourceFunc)SaturationDeactivate, this);
            gdk_threads_add_idle ((GSourceFunc)BrightnessDeactivate, this);
            gdk_threads_add_idle ((GSourceFunc)WhitebalanceDeactivate, this);
        } else {
            gdk_threads_add_idle ((GSourceFunc)ExposureActivate, this);
            gdk_threads_add_idle ((GSourceFunc)FramerateActivate, this);
            gdk_threads_add_idle ((GSourceFunc)GammaActivate, this);
            gdk_threads_add_idle ((GSourceFunc)GainActivate, this);
            gdk_threads_add_idle ((GSourceFunc)HdrActivate, this);
            gdk_threads_add_idle ((GSourceFunc)ColortempActivate, this);
            gdk_threads_add_idle ((GSourceFunc)SaturationActivate, this);
            gdk_threads_add_idle ((GSourceFunc)BrightnessActivate, this);
            gdk_threads_add_idle ((GSourceFunc)WhitebalanceActivate, this);
        }

        gdk_threads_add_idle ((GSourceFunc)RefreshComplete, this);
        m_numRefreshRequestsOutstanding++;
    } else {
        // If we are not the active tab, only bump the m_numRefreshRequestsOutstanding if there is not
        // one outstanding already; RefreshComplete will be scheduled when the tab becomes active
        if (!m_numRefreshRequestsOutstanding)m_numRefreshRequestsOutstanding++;
    }
}

void PxLControls::activate()
{
    // I have become the active tab.

    if (gCamera)
    {
        if (m_numRefreshRequestsOutstanding)
        {
            gdk_threads_add_idle ((GSourceFunc)ExposureActivate, this);
            gdk_threads_add_idle ((GSourceFunc)FramerateActivate, this);
            gdk_threads_add_idle ((GSourceFunc)GammaActivate, this);
            gdk_threads_add_idle ((GSourceFunc)GainActivate, this);
            gdk_threads_add_idle ((GSourceFunc)HdrActivate, this);
            gdk_threads_add_idle ((GSourceFunc)ColortempActivate, this);
            gdk_threads_add_idle ((GSourceFunc)SaturationActivate, this);
            gdk_threads_add_idle ((GSourceFunc)BrightnessActivate, this);
            gdk_threads_add_idle ((GSourceFunc)WhitebalanceActivate, this);
        }  else {
            // Start the pollers for any feature in the 'continuous' mode.  We know there isn't a one time
            // in progress, because we could not have changed tabs while the onetime dialog is active
            bool continuousExposureOn = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON(m_exposureContinous));
            if (continuousExposureOn)
            {
                gCamera->m_poller->pollAdd(exposureFuncs);
            }
            bool continuousGainOn = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON(m_gainContinous));
            if (continuousGainOn)
            {
                gCamera->m_poller->pollAdd(gainFuncs);
            }
            bool continuousFramerateOn = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON(m_framerateContinous));
            if (continuousFramerateOn)
            {
                gCamera->m_poller->pollAdd(framerateFuncs);
            }
        }
    } else {
        gdk_threads_add_idle ((GSourceFunc)ExposureDeactivate, this);
        gdk_threads_add_idle ((GSourceFunc)FramerateDeactivate, this);
        gdk_threads_add_idle ((GSourceFunc)GammaDeactivate, this);
        gdk_threads_add_idle ((GSourceFunc)GainDeactivate, this);
        gdk_threads_add_idle ((GSourceFunc)HdrDeactivate, this);
        gdk_threads_add_idle ((GSourceFunc)ColortempDeactivate, this);
        gdk_threads_add_idle ((GSourceFunc)SaturationDeactivate, this);
        gdk_threads_add_idle ((GSourceFunc)BrightnessDeactivate, this);
        gdk_threads_add_idle ((GSourceFunc)WhitebalanceDeactivate, this);
    }

    m_numRefreshRequestsOutstanding = 1; // As a safety mechanism, tab activation should assert value, it will be set to 0 when RefreshComplete
    gdk_threads_add_idle ((GSourceFunc)RefreshComplete, this);
}

void PxLControls::deactivate()
{
    // I am no longer the active tab.

    // Stop the pollers for any feature in continuous mode.  We will start them again if re-activated
    bool continuousExposureOn = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON(m_exposureContinous));
    if (continuousExposureOn && gCamera)
    {
        gCamera->m_poller->pollRemove(exposureFuncs);
    }
    bool continuousGainOn = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON(m_gainContinous));
    if (continuousGainOn && gCamera)
    {
        gCamera->m_poller->pollRemove(gainFuncs);
    }
    bool continuousFramerateOn = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON(m_framerateContinous));
    if (continuousFramerateOn && gCamera)
    {
        gCamera->m_poller->pollRemove(framerateFuncs);
    }
}

// indication that the app has transitioned to/from streaming state.
void PxLControls::streamChange (bool streaming)
{
    // The exposure and gain one time buttons need to enable and disable with the stream,
    // so update these controls.
    ExposureActivate (gControlsTab);
    GainActivate (gControlsTab);
}


/* ---------------------------------------------------------------------------
 * --   gtk thread callbacks - used to update controls
 * ---------------------------------------------------------------------------
 */

// Indicate that the refresh is no longer outstanding, it has completed.
static gboolean RefreshComplete (gpointer pData)
{
    PxLControls *pControls = (PxLControls *)pData;

    pControls->m_numRefreshRequestsOutstanding--;
    return false;
}

//
// Make exposure meaningless
static gboolean ExposureDeactivate (gpointer pData)
{
    PxLControls *pControls = (PxLControls *)pData;

    pControls->m_exposureSlider->deactivate();
    gtk_widget_set_sensitive (pControls->m_exposureOneTime, false);
    gtk_widget_set_sensitive (pControls->m_exposureContinous, false);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(pControls->m_exposureContinous), false);
    gtk_label_set_text (GTK_LABEL (pControls->m_exposureAutoMin), "");
    gtk_label_set_text (GTK_LABEL (pControls->m_exposureAutoMax), "");
    gtk_widget_set_sensitive (pControls->m_exposureSetAutoMin, false);
    gtk_widget_set_sensitive (pControls->m_exposureSetAutoMax, false);

    if (gCamera)
    {
        gCamera->m_poller->pollRemove(exposureFuncs);
    }

    return false;  //  Only run once....
}

//
// Assert all of the exposure controls
static gboolean ExposureActivate (gpointer pData)
{
    PxLControls *pControls = (PxLControls *)pData;

    bool oneTimeEnable = false;
    bool continuousEnable = false;
    bool continuousCurrentlyOn = false;
    bool supportsAutoLimits = false;
    bool streaming = false;

    PxLAutoLock lock(&gCameraLock);

    if (gCamera)
    {
        if (gCamera->supported(FEATURE_EXPOSURE))
        {
            float min, max, value;

            gtk_widget_set_sensitive (pControls->m_exposureContinous, false);

            if (gCamera->oneTimeSuppored(FEATURE_EXPOSURE)) oneTimeEnable = true;
            if (gCamera->continuousSupported(FEATURE_EXPOSURE))
            {
                continuousEnable = true;
                gCamera->getContinuousAuto(FEATURE_EXPOSURE, &continuousCurrentlyOn);
            }

            // Note that FEATURE_EXPOSURE deals with units of seconds, while our controls
            // use units of milliseconds, so we need to convert from one to the other.
            pControls->m_exposureSlider->activate(! continuousCurrentlyOn);
            gCamera->getRange(FEATURE_EXPOSURE, &min, &max);
            pControls->m_exposureSlider->setRange(min*1000, max*1000);
            gCamera->getValue(FEATURE_EXPOSURE, &value);
            pControls->m_exposureSlider->setValue(value*1000);

            if (gCamera->numParametersSupported(FEATURE_EXPOSURE) >= 3)
            {
                char cValue[40];

                supportsAutoLimits = true;
                gCamera->getAutoLimits(FEATURE_EXPOSURE, &min, &max);
                sprintf (cValue, "%5.3f",min*1000);
                gtk_label_set_text (GTK_LABEL (pControls->m_exposureAutoMin), cValue);
                sprintf (cValue, "%5.0f",max*1000);
                gtk_label_set_text (GTK_LABEL (pControls->m_exposureAutoMax), cValue);
            }

            streaming = gCamera->streaming();
        }
    }

    gtk_widget_set_sensitive (pControls->m_exposureOneTime, oneTimeEnable && !continuousCurrentlyOn && streaming);
    gtk_widget_set_sensitive (pControls->m_exposureContinous, continuousEnable);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(pControls->m_exposureContinous), continuousCurrentlyOn);
    pControls->m_exposureSlider->activate (! continuousCurrentlyOn);

    gtk_widget_set_sensitive (pControls->m_exposureSetAutoMin, supportsAutoLimits);
    gtk_widget_set_sensitive (pControls->m_exposureSetAutoMax, supportsAutoLimits);

    if (continuousCurrentlyOn)
    {
        // add our functions to the continuous poller
        gCamera->m_poller->pollAdd(exposureFuncs);
    }

    return false;  //  Only run once....
}

//
// Called periodically when doing continuous exposure updates -- reads the current value
static PXL_RETURN_CODE GetCurrentExposure()
{
    PXL_RETURN_CODE rc = ApiSuccess;

    PxLAutoLock lock(&gCameraLock);
    if (gCamera && gControlsTab)
    {
        // It's safe to assume the camera supports exposure, as this function will not be called
        // otherwise.  If we were to check via pCamera->supported (FEATURE_EXPOSURE) or
        // pCamera->continuousSupported (FEATURE_EXPSOURE), then that will perform a PxLGetCameraFeatures,
        // which is a lot of work for not.
        float exposureinSeconds = 0.0;
        rc = gCamera->getValue(FEATURE_EXPOSURE, &exposureinSeconds);
        if (API_SUCCESS(rc)) gControlsTab->m_exposureLast = exposureinSeconds * 1000;
    }

    return rc;
}

//
// Called periodically when doing continuous exposure updates -- updates the user controls
static void UpdateExposureControls()
{
    if (gCamera && gControlsTab)
    {
        PxLAutoLock lock(&gCameraLock);

        gControlsTab->m_exposureSlider->setValue(gControlsTab->m_exposureLast);

        bool continuousExposureOn = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON( gControlsTab->m_exposureContinous));
        bool onetimeExposureOn = false;
        if (gCamera->m_poller->polling (exposureFuncs))
        {
            gCamera->getOnetimeAuto (FEATURE_EXPOSURE, &onetimeExposureOn);
        }
        if (!continuousExposureOn && !onetimeExposureOn)
        {
            // No need to poll any longer
            gCamera->m_poller->pollRemove(exposureFuncs);
        }
    }
}

//
// Make framerate meaningless
static gboolean FramerateDeactivate (gpointer pData)
{
    PxLControls *pControls = (PxLControls *)pData;

    pControls->m_framerateSlider->deactivate();
    gtk_widget_set_sensitive (pControls->m_framerateFixed, false);
    gtk_widget_set_sensitive (pControls->m_framerateContinous, false);
    gtk_entry_set_text (GTK_ENTRY (pControls->m_framerateActual), "");
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(pControls->m_framerateFixed), false);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(pControls->m_framerateContinous), false);

    pControls->m_cameraSupportsActualFramerate = false;

    if (gCamera)
    {
        gCamera->m_poller->pollRemove(exposureFuncs);
        gCamera->m_poller->pollRemove(framerateFuncs);
    }

    return false;  //  Only run once....
}

//
// Assert all of the frmerate controls
static gboolean FramerateActivate (gpointer pData)
{
    PxLControls *pControls = (PxLControls *)pData;

    bool fixedEnable = false;
    bool fixedCurrentlyOn = false;
    bool continuousEnable = false;
    bool continuousCurrentlyOn = false;
    bool actualSupported = false;

    PxLAutoLock lock(&gCameraLock);

    if (gCamera)
    {
        if (gCamera->supported(FEATURE_FRAME_RATE))
        {
            float min, max, value, actualValue;

            gtk_widget_set_sensitive (pControls->m_framerateFixed, false);
            gtk_widget_set_sensitive (pControls->m_framerateContinous, false);

            if (gCamera->supported(FEATURE_SPECIAL_CAMERA_MODE))
            {
                if (API_SUCCESS (gCamera->getValue (FEATURE_SPECIAL_CAMERA_MODE, &value)))
                {
                    fixedEnable = true;
                    fixedCurrentlyOn = value == (float)FEATURE_SPECIAL_CAMERA_MODE_FIXED_FRAME_RATE;
                }
            }
            if (gCamera->continuousSupported(FEATURE_FRAME_RATE))
            {
                continuousEnable = true;
                gCamera->getContinuousAuto(FEATURE_FRAME_RATE, &continuousCurrentlyOn);
            }
            if (gCamera->supported(FEATURE_ACTUAL_FRAME_RATE)) actualSupported = true;

            gCamera->getRange(FEATURE_FRAME_RATE, &min, &max);
            pControls->m_framerateSlider->setRange(min, max);
            gCamera->getValue(FEATURE_FRAME_RATE, &value);
            pControls->m_framerateSlider->setValue(value);
            if (actualSupported)
            {
                gCamera->getValue(FEATURE_ACTUAL_FRAME_RATE, &actualValue);
            } else actualValue = value;
            char cActualValue[40];
            sprintf (cActualValue, "%5.3f",actualValue);
            gtk_entry_set_text (GTK_ENTRY (pControls->m_framerateActual), cActualValue);

            // And finally, re-display the Exposure label (if it's possible to change)
            bool warningRequired = gCamera->actualFrameRatelimiter() == FR_LIMITER_EXPOSURE;
            if (warningRequired)
            {
               gtk_label_set_text (GTK_LABEL (pControls->m_exposureLabel), "Exposure (ms) ** WARNING:Limits Frame Rate ** ");
            } else {
               gtk_label_set_text (GTK_LABEL (pControls->m_exposureLabel), "Exposure (ms)");
            }

        }
    }

    gtk_widget_set_sensitive (pControls->m_framerateFixed, fixedEnable);
    gtk_widget_set_sensitive (pControls->m_framerateContinous, continuousEnable && !fixedCurrentlyOn);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(pControls->m_framerateFixed), fixedCurrentlyOn);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(pControls->m_framerateContinous), continuousCurrentlyOn);
    pControls->m_framerateSlider->activate (! continuousCurrentlyOn && ! fixedCurrentlyOn);

    pControls->m_cameraSupportsActualFramerate = actualSupported;

    if (continuousCurrentlyOn)
    {
        // add our functions to the continuous poller
        gCamera->m_poller->pollAdd(framerateFuncs);
    }

    return false;  //  Only run once....
}

//
// Called periodically when doing continuous exposure updates -- reads the current value
static PXL_RETURN_CODE GetCurrentFramerate()
{
    PXL_RETURN_CODE rc = ApiSuccess;

    PxLAutoLock lock(&gCameraLock);
    if (gCamera && gControlsTab)
    {
        // It's safe to assume the camera supports framerate, as this function will not be called
        // otherwise.  If we were to check via pCamera->supported (FEATURE_FRAME_RATE) or
        // pCamera->continuousSupported (FEATURE_FRAME_RATE), then that will perform a PxLGetCameraFeatures,
        // which is a lot of work for not.
        float framerate = 0.0;
        rc = gCamera->getValue(FEATURE_FRAME_RATE, &framerate);
        gControlsTab->m_framerateLast = framerate;
        if (gControlsTab->m_cameraSupportsActualFramerate)
        {
            rc = gCamera->getValue(FEATURE_ACTUAL_FRAME_RATE, &framerate);
        }
        gControlsTab->m_framerateActualLast = framerate;
    }

    return rc;
}

//
// Called periodically when doing continuous framerate updates -- updates the user controls
static void UpdateFramerateControls()
{
    if (gCamera && gControlsTab)
    {
        PxLAutoLock lock(&gCameraLock);

        gControlsTab->m_framerateSlider->setValue(gControlsTab->m_framerateLast);
        char cValue[40];
        sprintf (cValue, "%5.3f",gControlsTab->m_framerateActualLast);
        gtk_entry_set_text (GTK_ENTRY (gControlsTab->m_framerateActual), cValue);
    }
}

//
// Make gamma meaningless
static gboolean GammaDeactivate (gpointer pData)
{
    PxLControls *pControls = (PxLControls *)pData;

    pControls->m_gammaSlider->deactivate();
    gtk_widget_set_sensitive (pControls->m_gammaEnable, false);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(pControls->m_gammaEnable), false);

    return false;  //  Only run once....
}

//
// Assert all of the gamma controls
static gboolean GammaActivate (gpointer pData)
{
    PxLControls *pControls = (PxLControls *)pData;

    bool supported = false;
    bool enabled = false;

    PxLAutoLock lock(&gCameraLock);

    if (gCamera)
    {
        if (gCamera->supported(FEATURE_GAMMA))
        {
            float min, max, value;

            supported = true;
            enabled = gCamera->enabled (FEATURE_GAMMA);

            gCamera->getRange(FEATURE_GAMMA, &min, &max);
            pControls->m_gammaSlider->setRange(min, max);
            gCamera->getValue(FEATURE_GAMMA, &value);
            pControls->m_gammaSlider->setValue(value);
        }
    }

    gtk_widget_set_sensitive (pControls->m_gammaEnable, supported);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(pControls->m_gammaEnable), supported && enabled);
    pControls->m_gammaSlider->activate (supported && enabled);

    return false;  //  Only run once....
}

//
// Make gain meaningless
static gboolean GainDeactivate (gpointer pData)
{
    PxLControls *pControls = (PxLControls *)pData;

    pControls->m_gainSlider->deactivate();
    gtk_widget_set_sensitive (pControls->m_gainOneTime, false);
    gtk_widget_set_sensitive (pControls->m_gainContinous, false);

    if (gCamera)
    {
        gCamera->m_poller->pollRemove(gainFuncs);
    }

    return false;  //  Only run once....
}

//
// Assert all of the the gain controls
static gboolean GainActivate (gpointer pData)
{
    PxLControls *pControls = (PxLControls *)pData;

    bool supported = false;
    bool oneTimeEnable = false;
    bool continuousEnable = false;
    bool continuousCurrentlyOn = false;
    bool streaming = false;

    PxLAutoLock lock(&gCameraLock);

    if (gCamera)
    {
        if (gCamera->supported(FEATURE_GAIN))
        {
            float min, max, value;

            supported = true;

            gtk_widget_set_sensitive (pControls->m_gainContinous, false);

            if (gCamera->oneTimeSuppored(FEATURE_GAIN)) oneTimeEnable = true;
            if (gCamera->continuousSupported(FEATURE_GAIN))
            {
                continuousEnable = true;
                gCamera->getContinuousAuto(FEATURE_GAIN, &continuousCurrentlyOn);
            }

            gCamera->getRange(FEATURE_GAIN, &min, &max);
            pControls->m_gainSlider->setRange(min, max);
            gCamera->getValue(FEATURE_GAIN, &value);
            pControls->m_gainSlider->setValue(value);

            streaming = gCamera->streaming();
        }
    }

    gtk_widget_set_sensitive (pControls->m_gainOneTime, oneTimeEnable && !continuousCurrentlyOn && streaming);
    gtk_widget_set_sensitive (pControls->m_gainContinous, continuousEnable);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(pControls->m_gainContinous), continuousCurrentlyOn);
    pControls->m_gainSlider->activate (supported);

    if (continuousCurrentlyOn)
    {
        // add our functions to the continuous poller
        gCamera->m_poller->pollAdd(gainFuncs);
    }

    return false;  //  Only run once....
}

//
// Called periodically when doing continuous gain updates -- reads the current value
static PXL_RETURN_CODE GetCurrentGain()
{
    PXL_RETURN_CODE rc = ApiSuccess;

    PxLAutoLock lock(&gCameraLock);
    if (gCamera && gControlsTab)
    {
        // It's safe to assume the camera supports gain, as this function will not be called
        // otherwise.  If we were to check via pCamera->supported (FEATURE_GAIN) or
        // pCamera->continuousSupported (FEATURE_GAIN), then that will perform a PxLGetCameraFeatures,
        // which is a lot of work for not.
        float gain = 0.0;
        rc = gCamera->getValue(FEATURE_GAIN, &gain);
        if (API_SUCCESS(rc)) gControlsTab->m_gainLast = gain;
    }

    return rc;
}

//
// Called periodically when doing continuous gain updates -- updates the user controls
static void UpdateGainControls()
{
    if (gCamera && gControlsTab)
    {
        PxLAutoLock lock(&gCameraLock);

        gControlsTab->m_gainSlider->setValue(gControlsTab->m_gainLast);

        bool continuousGainOn = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON( gControlsTab->m_gainContinous));
        bool onetimeGainOn = false;
        if (gCamera->m_poller->polling (gainFuncs))
        {
            gCamera->getOnetimeAuto (FEATURE_GAIN, &onetimeGainOn);
        }
        if (!continuousGainOn && !onetimeGainOn)
        {
            // No need to poll any longer
            gCamera->m_poller->pollRemove(gainFuncs);
        }
    }
}

//
// Make gain meaningless
static gboolean HdrDeactivate (gpointer pData)
{
    PxLControls *pControls = (PxLControls *)pData;

    gtk_widget_set_sensitive (pControls->m_hdrCombo, false);
    // Bugzilla.2138 -- the controlk is a combo box, not a togge.
    gtk_combo_box_set_active (GTK_COMBO_BOX(pControls->m_hdrCombo), false);

    return false;  //  Only run once....
}

//
// Assert all of the the gain controls
static gboolean HdrActivate (gpointer pData)
{
    PxLControls *pControls = (PxLControls *)pData;

    bool supported = false;
    int  currentHdrState = FEATURE_GAIN_HDR_MODE_NONE; // Assume that the HDR is disabled

    gtk_combo_box_text_remove_all (GTK_COMBO_BOX_TEXT(pControls->m_hdrCombo));

    PxLAutoLock lock(&gCameraLock);

    if (gCamera)
    {
        //
        // Step 1
        //     Always show the 'disabled' option in the dropdown
        gtk_combo_box_text_insert_text (GTK_COMBO_BOX_TEXT(pControls->m_hdrCombo),
                                        FEATURE_GAIN_HDR_MODE_NONE,
                                        "Disabled");

        if (gCamera->supported(FEATURE_GAIN_HDR))
        {
            float min, max, current;
            PXL_RETURN_CODE rc;

            supported = true;

            //
            // Step 2
            //      Cameras that support HDR, always support MODE_CAMERA
            gtk_combo_box_text_insert_text (GTK_COMBO_BOX_TEXT(pControls->m_hdrCombo),
                                            FEATURE_GAIN_HDR_MODE_CAMERA,
                                            "Camera Mode");

            //
            // Step 3
            //      Get the range of supported modes
            rc = gCamera->getRange(FEATURE_GAIN_HDR, &min, &max);
            if (API_SUCCESS(rc))
            {
                if (max >= FEATURE_GAIN_HDR_MODE_INTERLEAVED)
                {
                    // This camera supports interleaved HDR mode, so add it
                    gtk_combo_box_text_insert_text (GTK_COMBO_BOX_TEXT(pControls->m_hdrCombo),
                                                    FEATURE_GAIN_HDR_MODE_INTERLEAVED,
                                                    "Interleaved Mode");
                }
            }

            //
            // Step 4
            //      Get the current value
            rc = gCamera->getValue(FEATURE_GAIN_HDR, &current);
            if (API_SUCCESS(rc))
            {
                currentHdrState = (int)current;
            }
        }

        //
        // Step 5
        //      Set the dropdown to the current value
        gtk_combo_box_set_active (GTK_COMBO_BOX(pControls->m_hdrCombo),currentHdrState);

    }

    gtk_widget_set_sensitive (pControls->m_hdrCombo, supported);

    return false;  //  Only run once....
}

//
// Make colortemp meaningless
static gboolean ColortempDeactivate (gpointer pData)
{
    PxLControls *pControls = (PxLControls *)pData;

    pControls->m_colortempSlider->deactivate();
    gtk_widget_set_sensitive (pControls->m_colortempEnable, false);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(pControls->m_colortempEnable), false);

    return false;  //  Only run once....
}

//
// Assert all of the colortemp controls
static gboolean ColortempActivate (gpointer pData)
{
    PxLControls *pControls = (PxLControls *)pData;

    bool supported = false;
    bool enabled = false;

    PxLAutoLock lock(&gCameraLock);

    if (gCamera)
    {
        if (gCamera->supported(FEATURE_COLOR_TEMP))
        {
            float min, max, value;

            supported = true;
            enabled = gCamera->enabled (FEATURE_COLOR_TEMP);

            gCamera->getRange(FEATURE_COLOR_TEMP, &min, &max);
            pControls->m_colortempSlider->setRange(min, max);
            gCamera->getValue(FEATURE_COLOR_TEMP, &value);
            pControls->m_colortempSlider->setValue(value);
        }
    }

    gtk_widget_set_sensitive (pControls->m_colortempEnable, supported);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(pControls->m_colortempEnable), supported && enabled);
    pControls->m_colortempSlider->activate (supported && enabled);

    return false;  //  Only run once....
}

//
// Make saturation meaningless
static gboolean SaturationDeactivate (gpointer pData)
{
    PxLControls *pControls = (PxLControls *)pData;

    pControls->m_saturationSlider->deactivate();

    return false;  //  Only run once....
}

//
// Assert all of the the saturation controls
static gboolean SaturationActivate (gpointer pData)
{
    PxLControls *pControls = (PxLControls *)pData;

    bool supported = false;

    PxLAutoLock lock(&gCameraLock);

    if (gCamera)
    {
        if (gCamera->supported(FEATURE_SATURATION))
        {
            float min, max, value;

            supported = true;

            gCamera->getRange(FEATURE_SATURATION, &min, &max);
            pControls->m_saturationSlider->setRange(min, max);
            gCamera->getValue(FEATURE_SATURATION, &value);
            pControls->m_saturationSlider->setValue(value);
        }
    }

    pControls->m_saturationSlider->activate (supported);

    return false;  //  Only run once....
}

//
// Make brightness meaningless
static gboolean BrightnessDeactivate (gpointer pData)
{
    PxLControls *pControls = (PxLControls *)pData;

    pControls->m_brightnessSlider->deactivate();

    return false;  //  Only run once....
}

//
// Assert all of the brightness controls
static gboolean BrightnessActivate (gpointer pData)
{
    PxLControls *pControls = (PxLControls *)pData;

    bool supported = false;
    bool enabled = false;

    PxLAutoLock lock(&gCameraLock);

    if (gCamera)
    {
        if (gCamera->supported(FEATURE_BRIGHTNESS))
        {
            float min, max, value;

            supported = true;
            enabled = gCamera->enabled (FEATURE_BRIGHTNESS);

            gCamera->getRange(FEATURE_BRIGHTNESS, &min, &max);
            pControls->m_brightnessSlider->setRange(min, max);
            gCamera->getValue(FEATURE_BRIGHTNESS, &value);
            pControls->m_brightnessSlider->setValue(value);
        }
    }

    pControls->m_brightnessSlider->activate (supported && enabled);

    return false;  //  Only run once....
}

//
// Make whitebalance meaningless
static gboolean WhitebalanceDeactivate (gpointer pData)
{
    PxLControls *pControls = (PxLControls *)pData;

    pControls->m_redSlider->deactivate();
    pControls->m_greenSlider->deactivate();
    pControls->m_blueSlider->deactivate();
    gtk_widget_set_sensitive (pControls->m_whitebalanceOneTime, false);

    return false;  //  Only run once....
}

//
// Assert all of the exposure controls
static gboolean WhitebalanceActivate (gpointer pData)
{
    PxLControls *pControls = (PxLControls *)pData;

    bool supported = false;
    bool oneTimeEnable = false;

    PxLAutoLock lock(&gCameraLock);

    if (gCamera)
    {
        if (gCamera->supported(FEATURE_WHITE_SHADING))
        {
            float min, max;
            float red, green, blue;
            red = green = blue = 0.0f;

            supported = true;
            if (gCamera->oneTimeSuppored(FEATURE_WHITE_SHADING)) oneTimeEnable = true;

            gCamera->getRange(FEATURE_WHITE_SHADING, &min, &max);
            pControls->m_redSlider->setRange(min, max);
            pControls->m_greenSlider->setRange(min, max);
            pControls->m_blueSlider->setRange(min, max);
            gCamera->getWhiteBalanceValues(&red, &green, &blue);
            pControls->m_redSlider->setValue(red);
            pControls->m_greenSlider->setValue(green);
            pControls->m_blueSlider->setValue(blue);
        }
    }

    pControls->m_redSlider->activate(supported);
    pControls->m_greenSlider->activate(supported);
    pControls->m_blueSlider->activate(supported);
    gtk_widget_set_sensitive (pControls->m_whitebalanceOneTime, oneTimeEnable);

    return false;  //  Only run once....
}

//
// Called periodically when doing onetime whitebalance updates -- reads the current value
static PXL_RETURN_CODE GetCurrentWhitebalance()
{
    PXL_RETURN_CODE rc = ApiSuccess;

    PxLAutoLock lock(&gCameraLock);
    if (gCamera && gControlsTab)
    {
        float red, green, blue;
        red = green = blue = 0.0f;

        rc = gCamera->getWhiteBalanceValues(&red, &green, &blue);
        gControlsTab->m_redLast = red;
        gControlsTab->m_greenLast = green;
        gControlsTab->m_blueLast = blue;
    }

    return rc;
}

//
// Called periodically when doing continuous exposure updates -- updates the user controls
static void UpdateWhitebalanceControls()
{
    if (gCamera && gControlsTab)
    {
        PxLAutoLock lock(&gCameraLock);

        gControlsTab->m_redSlider->setValue(gControlsTab->m_redLast);
        gControlsTab->m_greenSlider->setValue(gControlsTab->m_greenLast);
        gControlsTab->m_blueSlider->setValue(gControlsTab->m_blueLast);

        bool onetimeWhitebalanceOn = false;
        if (gCamera->m_poller->polling (whitebalanceFuncs))
        {
            gCamera->getOnetimeAuto (FEATURE_WHITE_SHADING, &onetimeWhitebalanceOn);
        }
        if (!onetimeWhitebalanceOn)
        {
            // No need to poll any longer
            gCamera->m_poller->pollRemove(whitebalanceFuncs);

            // Update with the final value
            float red, green, blue;
            red = green = blue = 0.0f;

            gCamera->getWhiteBalanceValues(&red, &green, &blue);
            gControlsTab->m_redSlider->setValue(red);
            gControlsTab->m_greenSlider->setValue(green);
            gControlsTab->m_blueSlider->setValue(blue);

        }
    }
}


/* ---------------------------------------------------------------------------
 * --   Control functions from the Glade project
 * ---------------------------------------------------------------------------
 */

extern "C" void ExposureValueChanged
    (GtkWidget* widget, GdkEventExpose* event, gpointer userdata )
{
    if (! gCamera || ! gCameraSelectTab || !gControlsTab) return;
    if (gControlsTab->m_numRefreshRequestsOutstanding) return;

    PxLAutoLock lock(&gCameraLock);

    float newExposure;

    newExposure = gControlsTab->m_exposureSlider->getEditValue();

    // NOTE:
    //     FEATURE_EXPOSURE deals with units of seconds, while our controls
    //     use units of milliseconds, so we need to convert from one to the other.
    gCamera->setValue(FEATURE_EXPOSURE, newExposure/1000);

    // read it back again to see if the camera accepted it, or perhaps 'rounded it' to a
    // new value
    gCamera->getValue(FEATURE_EXPOSURE, &newExposure);
    gControlsTab->m_exposureSlider->setValue(newExposure*1000);

    // Update our frame rate control, as it may have changed
    gdk_threads_add_idle ((GSourceFunc)FramerateActivate, gControlsTab);

    // Update other tabs the next time they are activated
#if defined VIDEO_CAPTURE_SUPPORTED
    gVideoTab->refreshRequired(false);
#endif
    gLinkTab->refreshRequired(false);
    gAutoRoiTab->refreshRequired(false);
}

extern "C" void ExposureScaleChanged
    (GtkWidget* widget, GdkEventExpose* event, gpointer userdata )
{

    if (! gCamera || ! gCameraSelectTab || !gControlsTab) return;
    if (gControlsTab->m_numRefreshRequestsOutstanding) return;

    // we are only interested in changes to the scale from user input
    if (gControlsTab->m_exposureSlider->rangeChangeInProgress()) return;
    if (gControlsTab->m_exposureSlider->setIsInProgress()) return;

    float newExposure;

    PxLAutoLock lock(&gCameraLock);

    newExposure = gControlsTab->m_exposureSlider->getScaleValue();

    // NOTE:
    //     FEATURE_EXPOSURE deals with units of seconds, while our controls
    //     use units of milliseconds, so we need to convert from one to the other.
    gCamera->setValue(FEATURE_EXPOSURE, newExposure/1000.0f);
    // read it back again to see if the camera accepted it, or perhaps 'rounded it' to a
    // new value
    gCamera->getValue(FEATURE_EXPOSURE, &newExposure);
    gControlsTab->m_exposureSlider->setValue(newExposure*1000.0f);

    // Update our frame rate control, as it may have changed
    gdk_threads_add_idle ((GSourceFunc)FramerateActivate, gControlsTab);

    // Update other tabs the next time they are activated
#if defined VIDEO_CAPTURE_SUPPORTED
    gVideoTab->refreshRequired(false);
#endif
    gLinkTab->refreshRequired(false);
    gAutoRoiTab->refreshRequired(false);
}

extern "C" void ExposureAutoMinButtonPressed
    (GtkWidget* widget, GdkEventExpose* event, gpointer userdata )
{
    if (! gCamera || ! gCameraSelectTab || !gControlsTab) return;
    if (gControlsTab->m_numRefreshRequestsOutstanding) return;

    PxLAutoLock lock(&gCameraLock);

    float currentExposure;
    char cValue[40];

    currentExposure = gControlsTab->m_exposureSlider->getEditValue();

    sprintf (cValue, "%5.0f",currentExposure);
    gtk_label_set_text (GTK_LABEL (gControlsTab->m_exposureAutoMin), cValue);

}

extern "C" void ExposureAutoMaxButtonPressed
    (GtkWidget* widget, GdkEventExpose* event, gpointer userdata )
{
    if (! gCamera || ! gCameraSelectTab || !gControlsTab) return;
    if (gControlsTab->m_numRefreshRequestsOutstanding) return;

    PxLAutoLock lock(&gCameraLock);

    float currentExposure;
    char cValue[40];

    currentExposure = gControlsTab->m_exposureSlider->getEditValue();

    sprintf (cValue, "%5.0f",currentExposure);
    gtk_label_set_text (GTK_LABEL (gControlsTab->m_exposureAutoMax), cValue);

}

extern "C" void ExposureOneTimeButtonPressed
    (GtkWidget* widget, GdkEventExpose* event, gpointer userdata )
{
    if (! gCamera || ! gCameraSelectTab || !gControlsTab) return;
    if (!gCamera->oneTimeSuppored(FEATURE_EXPOSURE)) return;
    if (gControlsTab->m_numRefreshRequestsOutstanding) return;

    PxLAutoLock lock(&gCameraLock);

    // Use the limits if they are supported.  We can easily tell this by checking
    // one of the limit buttons to see if it is enabled.
    bool autoLimitsSupported =  gtk_widget_get_sensitive (gControlsTab->m_exposureSetAutoMin);

    if (autoLimitsSupported)
    {
        float min = atof (gtk_label_get_text (GTK_LABEL (gControlsTab->m_exposureAutoMin)));
        float max = atof (gtk_label_get_text (GTK_LABEL (gControlsTab->m_exposureAutoMax)));
        gOnetimeDialog->initiate(FEATURE_EXPOSURE, 500, min / 1000.0f, max / 1000.0f); // Pool every 500 ms
    } else {
        gOnetimeDialog->initiate(FEATURE_EXPOSURE, 500); // Pool every 500 ms
    }
    // Also add a poller so that the slider and the edit control also update as the
    // one time is performed.
    gCamera->m_poller->pollAdd(exposureFuncs);

    // Update other tabs the next time they are activated
    gAutoRoiTab->refreshRequired(false);
}

extern "C" void ExposureContinuousToggled
    (GtkWidget* widget, GdkEventExpose* event, gpointer userdata )
{
    PXL_RETURN_CODE rc;

    if (! gCamera || ! gCameraSelectTab || !gControlsTab) return;
    if (!gCamera->continuousSupported(FEATURE_EXPOSURE)) return;
    if (gControlsTab->m_numRefreshRequestsOutstanding) return;

    bool continuousOn = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON(gControlsTab->m_exposureContinous));
    // Use the limits if they are supported.  We can easily tell this by checking
    // one of the limit buttons to see if it is enabled.
    bool autoLimitsSupported =  gtk_widget_get_sensitive (gControlsTab->m_exposureSetAutoMin);

    PxLAutoLock lock(&gCameraLock);

    if (autoLimitsSupported)
    {
        float min = atof (gtk_label_get_text (GTK_LABEL (gControlsTab->m_exposureAutoMin)));
        float max = atof (gtk_label_get_text (GTK_LABEL (gControlsTab->m_exposureAutoMax)));
        rc = gCamera->setContinuousAuto(FEATURE_EXPOSURE, continuousOn, min / 1000.0f, max / 1000.0f);
    } else {
        rc = gCamera->setContinuousAuto(FEATURE_EXPOSURE, continuousOn);
    }
    if (!API_SUCCESS(rc)) return;

    // ensure the Make the slider and the oneTime buttons are only writable if we are not continuously adjusting
    gControlsTab->m_exposureSlider->activate (!continuousOn);
    // Also, onTime should only ever be if while streaming.
    gtk_widget_set_sensitive (gControlsTab->m_exposureOneTime, !continuousOn  && gCamera->streaming());

    if (continuousOn)
    {
        // add our functions to the continuous poller
        gCamera->m_poller->pollAdd(exposureFuncs);
    } else {
        // remove our functions from the continuous poller
        gCamera->m_poller->pollRemove(exposureFuncs);
    }

    // Update other tabs the next time they are activated
    gAutoRoiTab->refreshRequired(false);
}

extern "C" void FramerateValueChanged
    (GtkWidget* widget, GdkEventExpose* event, gpointer userdata )
{
    if (! gCamera || ! gCameraSelectTab || !gControlsTab) return;
    if (gControlsTab->m_numRefreshRequestsOutstanding) return;

    PxLAutoLock lock(&gCameraLock);

    float newFramerate;

    newFramerate = gControlsTab->m_framerateSlider->getEditValue();

    gCamera->setValue(FEATURE_FRAME_RATE, newFramerate);

    // Update our entire framerate control, as we may need to updated the frame rate limit warning
    gdk_threads_add_idle ((GSourceFunc)FramerateActivate, gControlsTab);

    // Let other interested tabs know the frame rate has changed
#if defined VIDEO_CAPTURE_SUPPORTED
    gVideoTab->refreshRequired(false);
#endif
    gLinkTab->refreshRequired(false);
}

extern "C" void FramerateScaleChanged
    (GtkWidget* widget, GdkEventExpose* event, gpointer userdata )
{

    if (! gCamera || ! gCameraSelectTab || !gControlsTab) return;
    if (gControlsTab->m_numRefreshRequestsOutstanding) return;

    // we are only interested in changes to the scale from user input
    if (gControlsTab->m_framerateSlider->rangeChangeInProgress()) return;
    if (gControlsTab->m_framerateSlider->setIsInProgress()) return;

    float newFramerate;

    newFramerate = gControlsTab->m_framerateSlider->getScaleValue();

    PxLAutoLock lock(&gCameraLock);

    gCamera->setValue(FEATURE_FRAME_RATE, newFramerate);

    // Update our entire framerate control, as we may need to updated the frame rate limit warning
    gdk_threads_add_idle ((GSourceFunc)FramerateActivate, gControlsTab);

    // Let other interested tabs know the frame rate has changed
#if defined VIDEO_CAPTURE_SUPPORTED
    gVideoTab->refreshRequired(false);
#endif
    gLinkTab->refreshRequired(false);
}

extern "C" void FramerateFixedToggled
    (GtkWidget* widget, GdkEventExpose* event, gpointer userdata )
{
    PXL_RETURN_CODE rc;

    if (! gCamera || ! gCameraSelectTab || !gControlsTab) return;
    if (!gCamera->continuousSupported(FEATURE_EXPOSURE)) return;
    if (gControlsTab->m_numRefreshRequestsOutstanding) return;

    bool fixedOn = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON(gControlsTab->m_framerateFixed));

    PxLAutoLock lock(&gCameraLock);

    float cameraMode = (float)(fixedOn ? FEATURE_SPECIAL_CAMERA_MODE_FIXED_FRAME_RATE : FEATURE_SPECIAL_CAMERA_MODE_NONE);
    rc = gCamera->setValue(FEATURE_SPECIAL_CAMERA_MODE, cameraMode);
    if (!API_SUCCESS(rc)) return;

    // changing camera modes is a big deal to the camera, reassert all of the controls to ensure they
    // are current.
    gControlsTab->refreshRequired(false);
    gStreamTab->refreshRequired(false);
#if defined VIDEO_CAPTURE_SUPPORTED
    gVideoTab->refreshRequired(false);
#endif
}

extern "C" void FramerateContinuousToggled
    (GtkWidget* widget, GdkEventExpose* event, gpointer userdata )
{
    PXL_RETURN_CODE rc;

    if (! gCamera || ! gCameraSelectTab || !gControlsTab) return;
    if (!gCamera->continuousSupported(FEATURE_FRAME_RATE)) return;
    if (gControlsTab->m_numRefreshRequestsOutstanding) return;

    bool continuousOn = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON(gControlsTab->m_framerateContinous));

    PxLAutoLock lock(&gCameraLock);

    rc = gCamera->setContinuousAuto(FEATURE_FRAME_RATE, continuousOn);
    if (!API_SUCCESS(rc)) return;

    // ensure the slider and the fixed button are only writable if we are not continuously adjusting
    gControlsTab->m_framerateSlider->activate (!continuousOn);
    gtk_widget_set_sensitive (gControlsTab->m_framerateFixed, !continuousOn);

    if (continuousOn)
    {
        // add our functions to the continuous poller
        gCamera->m_poller->pollAdd(framerateFuncs);
    } else {
        // remove our functions from the continuous poller
        gCamera->m_poller->pollRemove(framerateFuncs);
    }

    // Update our entire framerate control, as we may need to updated the frame rate limit warning
    gdk_threads_add_idle ((GSourceFunc)FramerateActivate, gControlsTab);

    // Let other interested tabs know the frame rate has changed
#if defined VIDEO_CAPTURE_SUPPORTED
    gVideoTab->refreshRequired(false);
#endif
}

extern "C" void GammaValueChanged
    (GtkWidget* widget, GdkEventExpose* event, gpointer userdata )
{
    if (! gCamera || ! gCameraSelectTab || !gControlsTab) return;
    if (gControlsTab->m_numRefreshRequestsOutstanding) return;

    PxLAutoLock lock(&gCameraLock);

    float newValue;

    newValue = gControlsTab->m_gammaSlider->getEditValue();

    PXL_RETURN_CODE rc = gCamera->setValue(FEATURE_GAMMA, newValue);

    // Read it back to see if the camera did any 'rounding'
    float newRoundedValue = newValue;
    rc = gCamera->getValue(FEATURE_GAMMA, &newRoundedValue);
    if (API_SUCCESS(rc))
    {
        gControlsTab->m_gammaSlider->setValue(newRoundedValue);
    }
}

extern "C" void GammaScaleChanged
    (GtkWidget* widget, GdkEventExpose* event, gpointer userdata )
{

    if (! gCamera || ! gCameraSelectTab || !gControlsTab) return;
    if (gControlsTab->m_numRefreshRequestsOutstanding) return;

    // we are only interested in changes to the scale from user input
    if (gControlsTab->m_gammaSlider->rangeChangeInProgress()) return;
    if (gControlsTab->m_gammaSlider->setIsInProgress()) return;

    float newValue;

    newValue = gControlsTab->m_gammaSlider->getScaleValue();

    PxLAutoLock lock(&gCameraLock);

    PXL_RETURN_CODE rc = gCamera->setValue(FEATURE_GAMMA, newValue);

    // Read it back to see if the camera did any 'rounding'
    if (API_SUCCESS(rc))
    {
        float newRoundedValue = newValue;
        rc = gCamera->getValue(FEATURE_GAMMA, &newRoundedValue);
        if (API_SUCCESS(rc))
        {
            gControlsTab->m_gammaSlider->setValue(newRoundedValue);
        }
    }
}

extern "C" void GammaEnableToggled
    (GtkWidget* widget, GdkEventExpose* event, gpointer userdata )
{
    if (! gCamera || ! gCameraSelectTab || !gControlsTab) return;
    if (gControlsTab->m_numRefreshRequestsOutstanding) return;

    bool enable = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON(gControlsTab->m_gammaEnable));

    PxLAutoLock lock(&gCameraLock);

    if (enable)
    {
        float currentValue;

        currentValue = gControlsTab->m_gammaSlider->getScaleValue();
        gControlsTab->m_gammaSlider->activate(true);
        gCamera->setValue (FEATURE_GAMMA, currentValue);
    } else {
        gCamera->disable(FEATURE_GAMMA);
        gControlsTab->m_gammaSlider->activate(false);
    }
}

extern "C" void GainValueChanged
    (GtkWidget* widget, GdkEventExpose* event, gpointer userdata )
{
    if (! gCamera || ! gCameraSelectTab || !gControlsTab) return;
    if (gControlsTab->m_numRefreshRequestsOutstanding) return;

    PxLAutoLock lock(&gCameraLock);

    float newValue;

    newValue = gControlsTab->m_gainSlider->getEditValue();

    PXL_RETURN_CODE rc = gCamera->setValue(FEATURE_GAIN, newValue);

    // Read it back to see if the camera did any 'rounding'
    float newRoundedValue = newValue;
    rc = gCamera->getValue(FEATURE_GAIN, &newRoundedValue);
    if (API_SUCCESS(rc))
    {
        gControlsTab->m_gainSlider->setValue(newRoundedValue);
    }

    // Update other tabs the next time they are activated
    gAutoRoiTab->refreshRequired(false);
}

extern "C" void GainScaleChanged
    (GtkWidget* widget, GdkEventExpose* event, gpointer userdata )
{

    if (! gCamera || ! gCameraSelectTab || !gControlsTab) return;
    if (gControlsTab->m_numRefreshRequestsOutstanding) return;

    // we are only interested in changes to the scale from user input
    if (gControlsTab->m_gainSlider->rangeChangeInProgress()) return;
    if (gControlsTab->m_gainSlider->setIsInProgress()) return;

    float newValue;

    newValue = gControlsTab->m_gainSlider->getScaleValue();

    PxLAutoLock lock(&gCameraLock);

    PXL_RETURN_CODE rc = gCamera->setValue(FEATURE_GAIN, newValue);

    // Read it back to see if the camera did any 'rounding'
    if (API_SUCCESS(rc))
    {
        float newRoundedValue = newValue;
        rc = gCamera->getValue(FEATURE_GAIN, &newRoundedValue);
        if (API_SUCCESS(rc))
        {
            gControlsTab->m_gainSlider->setValue(newRoundedValue);
        }
    }

    // Update other tabs the next time they are activated
    gAutoRoiTab->refreshRequired(false);
}

extern "C" void GainOneTimeButtonPressed
    (GtkWidget* widget, GdkEventExpose* event, gpointer userdata )
{
    if (! gCamera || ! gCameraSelectTab || !gControlsTab) return;
    if (!gCamera->oneTimeSuppored(FEATURE_GAIN)) return;
    if (gControlsTab->m_numRefreshRequestsOutstanding) return;

    PxLAutoLock lock(&gCameraLock);

    gOnetimeDialog->initiate(FEATURE_GAIN, 500); // Pool every 500 ms
    // Also add a poller so that the slider and the edit control also update as the
    // one time is performed.
    gCamera->m_poller->pollAdd(gainFuncs);

    // Update other tabs the next time they are activated
    gAutoRoiTab->refreshRequired(false);
}

extern "C" void GainContinuousToggled
    (GtkWidget* widget, GdkEventExpose* event, gpointer userdata )
{
    PXL_RETURN_CODE rc;

    if (! gCamera || ! gCameraSelectTab || !gControlsTab) return;
    if (!gCamera->continuousSupported(FEATURE_GAIN)) return;
    if (gControlsTab->m_numRefreshRequestsOutstanding) return;

    bool continuousOn = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON(gControlsTab->m_gainContinous));

    PxLAutoLock lock(&gCameraLock);

    rc = gCamera->setContinuousAuto(FEATURE_GAIN, continuousOn);
    if (!API_SUCCESS(rc)) return;

    // ensure the Make the slider and the oneTime buttons are only writable if we are not continuously adjusting
    gControlsTab->m_gainSlider->activate (!continuousOn);
    // Also, onTime should only ever be if while streaming.
    gtk_widget_set_sensitive (gControlsTab->m_gainOneTime, !continuousOn);

    if (continuousOn)
    {
        // add our functions to the continuous poller
        gCamera->m_poller->pollAdd(gainFuncs);
    } else {
        // remove our functions from the continuous poller
        gCamera->m_poller->pollRemove(gainFuncs);
    }

    // Update other tabs the next time they are activated
    gAutoRoiTab->refreshRequired(false);
}

extern "C" void NewHdrSelected
    (GtkWidget* widget, GdkEventExpose* event, gpointer userdata )
{
    if (! gCamera || ! gCameraSelectTab || !gControlsTab) return;
    if (!gCamera->continuousSupported(FEATURE_GAIN)) return;
    if (gControlsTab->m_numRefreshRequestsOutstanding) return;

    PxLAutoLock lock(&gCameraLock);

    gint requestedHdrMode = gtk_combo_box_get_active (GTK_COMBO_BOX(gControlsTab->m_hdrCombo));

    // Bugzilla.2180 -- activate/de-activate the gain controls with HDR mode changes
    if (requestedHdrMode == FEATURE_GAIN_HDR_MODE_NONE)
    {
        gCamera->disable (FEATURE_GAIN_HDR);
        gdk_threads_add_idle ((GSourceFunc)GainActivate, gControlsTab);
    } else {
        gCamera->setValue(FEATURE_GAIN_HDR, (float)requestedHdrMode);
        gdk_threads_add_idle ((GSourceFunc)GainDeactivate, gControlsTab);
    }

    // Bugzilla.1600 Update our frame rate control, as it may have changed
    gdk_threads_add_idle ((GSourceFunc)FramerateActivate, gControlsTab);

    // Update other tabs the next time they are activated
#if defined VIDEO_CAPTURE_SUPPORTED
    gVideoTab->refreshRequired(false);
#endif
    gLinkTab->refreshRequired(false);
}

extern "C" void ColortempValueChanged
    (GtkWidget* widget, GdkEventExpose* event, gpointer userdata )
{
    if (! gCamera || ! gCameraSelectTab || !gControlsTab) return;
    if (gControlsTab->m_numRefreshRequestsOutstanding) return;

    PxLAutoLock lock(&gCameraLock);

    float newValue;

    newValue = gControlsTab->m_colortempSlider->getEditValue();

    PXL_RETURN_CODE rc = gCamera->setValue(FEATURE_COLOR_TEMP, newValue);

    // Read it back to see if the camera did any 'rounding'
    float newRoundedValue = newValue;
    rc = gCamera->getValue(FEATURE_COLOR_TEMP, &newRoundedValue);
    if (API_SUCCESS(rc))
    {
        gControlsTab->m_colortempSlider->setValue(newRoundedValue);
    }
}

extern "C" void ColortempScaleChanged
    (GtkWidget* widget, GdkEventExpose* event, gpointer userdata )
{

    if (! gCamera || ! gCameraSelectTab || !gControlsTab) return;
    if (gControlsTab->m_numRefreshRequestsOutstanding) return;

    // we are only interested in changes to the scale from user input
    if (gControlsTab->m_colortempSlider->rangeChangeInProgress()) return;
    if (gControlsTab->m_colortempSlider->setIsInProgress()) return;

    float newValue;

    newValue = gControlsTab->m_colortempSlider->getScaleValue();

    PxLAutoLock lock(&gCameraLock);

    PXL_RETURN_CODE rc = gCamera->setValue(FEATURE_COLOR_TEMP, newValue);

    // Read it back to see if the camera did any 'rounding'
    if (API_SUCCESS(rc))
    {
        float newRoundedValue = newValue;
        rc = gCamera->getValue(FEATURE_COLOR_TEMP, &newRoundedValue);
        if (API_SUCCESS(rc))
        {
            gControlsTab->m_colortempSlider->setValue(newRoundedValue);
        }
    }
}

extern "C" void ColortempEnableToggled
    (GtkWidget* widget, GdkEventExpose* event, gpointer userdata )
{
    if (! gCamera || ! gCameraSelectTab || !gControlsTab) return;
    if (gControlsTab->m_numRefreshRequestsOutstanding) return;

    bool enable = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON(gControlsTab->m_colortempEnable));

    PxLAutoLock lock(&gCameraLock);

    if (enable)
    {
        float currentValue;

        currentValue = gControlsTab->m_colortempSlider->getScaleValue();
        gControlsTab->m_colortempSlider->activate(true);
        gCamera->setValue (FEATURE_COLOR_TEMP, currentValue);
    } else {
        gCamera->disable(FEATURE_COLOR_TEMP);
        gControlsTab->m_colortempSlider->activate(false);
    }
}

extern "C" void SaturationValueChanged
    (GtkWidget* widget, GdkEventExpose* event, gpointer userdata )
{
    if (! gCamera || ! gCameraSelectTab || !gControlsTab) return;
    if (gControlsTab->m_numRefreshRequestsOutstanding) return;

    PxLAutoLock lock(&gCameraLock);

    float newValue;

    newValue = gControlsTab->m_saturationSlider->getEditValue();

    PXL_RETURN_CODE rc = gCamera->setValue(FEATURE_SATURATION, newValue);

    // Read it back to see if the camera did any 'rounding'
    float newRoundedValue = newValue;
    rc = gCamera->getValue(FEATURE_SATURATION, &newRoundedValue);
    if (API_SUCCESS(rc))
    {
        gControlsTab->m_saturationSlider->setValue(newRoundedValue);
    }
}

extern "C" void SaturationScaleChanged
    (GtkWidget* widget, GdkEventExpose* event, gpointer userdata )
{

    if (! gCamera || ! gCameraSelectTab || !gControlsTab) return;
    if (gControlsTab->m_numRefreshRequestsOutstanding) return;

    // we are only interested in changes to the scale from user input
    if (gControlsTab->m_saturationSlider->rangeChangeInProgress()) return;
    if (gControlsTab->m_saturationSlider->setIsInProgress()) return;

    float newValue;

    newValue = gControlsTab->m_saturationSlider->getScaleValue();

    PxLAutoLock lock(&gCameraLock);

    PXL_RETURN_CODE rc = gCamera->setValue(FEATURE_SATURATION, newValue);

    // Read it back to see if the camera did any 'rounding'
    float newRoundedValue = newValue;
    rc = gCamera->getValue(FEATURE_SATURATION, &newRoundedValue);
    if (API_SUCCESS(rc))
    {
        gControlsTab->m_saturationSlider->setValue(newRoundedValue);
    }
}

extern "C" void BrightnessValueChanged
    (GtkWidget* widget, GdkEventExpose* event, gpointer userdata )
{
    if (! gCamera || ! gCameraSelectTab || !gControlsTab) return;
    if (gControlsTab->m_numRefreshRequestsOutstanding) return;

    PxLAutoLock lock(&gCameraLock);

    float newValue;

    newValue = gControlsTab->m_brightnessSlider->getEditValue();

    PXL_RETURN_CODE rc = gCamera->setValue(FEATURE_BRIGHTNESS, newValue);

    // Read it back to see if the camera did any 'rounding'
    float newRoundedValue = newValue;
    rc = gCamera->getValue(FEATURE_BRIGHTNESS, &newRoundedValue);
    if (API_SUCCESS(rc))
    {
        gControlsTab->m_brightnessSlider->setValue(newRoundedValue);
    }
}

extern "C" void BrightnessScaleChanged
    (GtkWidget* widget, GdkEventExpose* event, gpointer userdata )
{

    if (! gCamera || ! gCameraSelectTab || !gControlsTab) return;
    if (gControlsTab->m_numRefreshRequestsOutstanding) return;

    // we are only interested in changes to the scale from user input
    if (gControlsTab->m_brightnessSlider->rangeChangeInProgress()) return;
    if (gControlsTab->m_brightnessSlider->setIsInProgress()) return;

    float newValue;

    newValue = gControlsTab->m_brightnessSlider->getScaleValue();

    PxLAutoLock lock(&gCameraLock);

    PXL_RETURN_CODE rc = gCamera->setValue(FEATURE_BRIGHTNESS, newValue);

    // Read it back to see if the camera did any 'rounding'
    if (API_SUCCESS(rc))
    {
        float newRoundedValue = newValue;
        rc = gCamera->getValue(FEATURE_BRIGHTNESS, &newRoundedValue);
        if (API_SUCCESS(rc))
        {
            gControlsTab->m_brightnessSlider->setValue(newRoundedValue);
        }
    }
}

extern "C" void WhitebalanceValueChanged
    (GtkWidget* widget, GdkEventExpose* event, gpointer userdata )
{
    if (! gCamera || ! gCameraSelectTab || !gControlsTab) return;
    if (gControlsTab->m_numRefreshRequestsOutstanding) return;

    PxLAutoLock lock(&gCameraLock);

    float red, green, blue;

    red = gControlsTab->m_redSlider->getEditValue();
    green = gControlsTab->m_greenSlider->getEditValue();
    blue = gControlsTab->m_blueSlider->getEditValue();

    gCamera->setWhiteBalanceValues(red, green, blue);

    // read it back again to see if the camera accepted it, or perhaps 'rounded it' to a
    // new value
    gCamera->getWhiteBalanceValues(&red, &green, &blue);
    gControlsTab->m_redSlider->setValue(red);
    gControlsTab->m_greenSlider->setValue(green);
    gControlsTab->m_blueSlider->setValue(blue);

    // Update other tabs the next time they are activated
    gAutoRoiTab->refreshRequired(false);
}

extern "C" void WhitebalanceScaleChanged
    (GtkWidget* widget, GdkEventExpose* event, gpointer userdata )
{

    if (! gCamera || ! gCameraSelectTab || !gControlsTab) return;
    if (gControlsTab->m_numRefreshRequestsOutstanding) return;

    // we are only interested in changes to the scale from user input
    if (gControlsTab->m_redSlider->rangeChangeInProgress()) return;
    if (gControlsTab->m_greenSlider->rangeChangeInProgress()) return;
    if (gControlsTab->m_blueSlider->rangeChangeInProgress()) return;
    if (gControlsTab->m_redSlider->setIsInProgress()) return;
    if (gControlsTab->m_greenSlider->setIsInProgress()) return;
    if (gControlsTab->m_blueSlider->setIsInProgress()) return;

    float red, green, blue;

    red = gControlsTab->m_redSlider->getScaleValue();
    green = gControlsTab->m_greenSlider->getScaleValue();
    blue = gControlsTab->m_blueSlider->getScaleValue();

    PxLAutoLock lock(&gCameraLock);

    gCamera->setWhiteBalanceValues(red, green, blue);
    // read it back again to see if the camera accepted it, or perhaps 'rounded it' to a
    // new value
    gCamera->getWhiteBalanceValues(&red, &green, &blue);
    gControlsTab->m_redSlider->setValue(red);
    gControlsTab->m_greenSlider->setValue(green);
    gControlsTab->m_blueSlider->setValue(blue);

    // Update other tabs the next time they are activated
    gAutoRoiTab->refreshRequired(false);
}

extern "C" void WhitebalanceOneTimeButtonPressed
    (GtkWidget* widget, GdkEventExpose* event, gpointer userdata )
{
    if (! gCamera || ! gCameraSelectTab || !gControlsTab) return;
    if (!gCamera->oneTimeSuppored(FEATURE_WHITE_SHADING)) return;
    if (gControlsTab->m_numRefreshRequestsOutstanding) return;

    PxLAutoLock lock(&gCameraLock);

    gOnetimeDialog->initiate(FEATURE_WHITE_SHADING, 500); // Pool every 500 ms
    // Also add a poller so that the slider and the edit control also update as the
    // one time is performed.
    gCamera->m_poller->pollAdd(whitebalanceFuncs);

    // Update other tabs the next time they are activated
    gAutoRoiTab->refreshRequired(false);
}






