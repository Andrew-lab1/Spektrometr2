/***************************************************************************
 *
 *     File: gpio.cpp
 *
 *     Description:
 *        Controls for the 'GPIO' tab  in CaptureOEM.
 */

#include <glib.h>
#include <vector>
#include <algorithm>
#include <string>
#include "gpio.h"
#include "camera.h"
#include "captureOEM.h"
#include "cameraSelect.h"
#include "stream.h"
#include "helpers.h"

using namespace std;

extern PxLGpio         *gGpioTab;
extern PxLCameraSelect *gCameraSelectTab;
extern PxLStream       *gStreamTab;
extern GtkWindow       *gTopLevelWindow;

// 
#define TRIGGER_TYPE_INVALID (-1)                   // Used to show there is no hardware trigger possible
#define TRIGGER_TYPE_NONE TRIGGER_TYPE_FREE_RUNNING // Used show no trigger chrrently enabled

static U32 PxLEventCallback (
        HANDLE hCamera,
		U32    eventId,
		double eventTimestamp,
		U32    numDataBytes,
        LPVOID pData,
        LPVOID pContext);

//
// Local prototypes.
//    UI updates can only be done from a gtk thread -- these routines are gtk 'idle' threads
//    and as such are 'UI update safe'. For each 'feature', there there the following functions:
//       . {featureXXX}Deactivate - Makes the controls meaningless (including greying them out)
//       . {featreuXXX}Activate - updates the controls with values from the camera
static gboolean  RefreshComplete (gpointer pData);
static gboolean  TriggerDeactivate (gpointer pData);
static gboolean  TriggerActivate (gpointer pData);
static gboolean  GpioDeactivate (gpointer pData);
static gboolean  GpioActivate (gpointer pData);
static gboolean  ActionsActivate (gpointer pData);
static gboolean  ActionsDeactivate (gpointer pData);
static gboolean  EventsActivate (gpointer pData);
static gboolean  EventsDeactivate (gpointer pData);
static void      UpdateTriggerInfo (PxLTriggerInfo& info, vector<int>& supportedHwTriggerModes);
static void      UpdateGpioInfo (PxLGpioInfo& info);

PXL_RETURN_CODE  GetCurrentGpio();
void             UpdateGpiStatus();
const PxLFeaturePollFunctions gpInputPoll (GetCurrentGpio, UpdateGpiStatus);

static const char* const PxLTriggerModeDescriptions[] = {
  "Mode 0\n\n"
  "Start integration at external trigger's\n"
  "leading edge.  Integration time is\n"
  "defined by FEATURE_SHUTTER.",

  "Mode 1\n\n"
  "Start integration at external trigger's\n"
  "leading edge and ends at the trigger's\n"
  "trailing edge.",

  "Mode 14\n\n"
  "The camera will capture Number frames\n"
  "after a trigger at the current\n"
  "integration time and frame rate.  If\n"
  "Number is set to 0 (if supported by\n"
  "the camera), the stream will continue\n"
  "until stopped by the user. "
};

// Indexed by GPIO_MODE_XXXX from PixeLINKTypes.h
static const char * const PxLGpioModeStrings[] =
{
   "Strobe",
   "Normal",
   "Pulse",
   "Busy",
   "Flash",
   "Input",
   "ActionStrobe",
   "ActionNormal",
   "ActionPulse",
   "HardwareTrigger",
};

// Indexed by GPIO_MODE_XXXX from PixeLINKTypes.h
static const char* const PxLGpioModeDescriptions[] = {
   "Mode Strobe\n\n"
   "The GPO is set after a trigger occurs.\n"
   "The GPO pulse occurs Delay milliseconds\n"
   "from the trigger and is Duration\n"
   "milliseconds in length.",

   "Mode Normal\n\n"
   "The GPO is set to either low or high,\n"
   "depending on the value of Polarity.",

   "Mode Pulse\n\n"
   "The GPO is pulsed whenever it is turned\n"
   "on. The GPO outputs Number of pulses\n "
   "pulses of Duration milliseconds in\n"
   "length, separated by Interval\n"
   "milliseconds.",

   "Mode Busy\n\n"
   "The GPO is set whenever the camera is\n"
   "unable to respond to a trigger. ",

   "Mode Flash\n\n"
   "The GPO signal is set once the sensor\n"
   "has been reset and starts integrating,\n"
   "and will be deactivated at the end of\n"
   "the exposure time as readout of the\n "
   "array commences.",

   "Mode Input\n\n"
   "Function as a General Purpose Input.\n"
   "The value of the input line is returned\n"
   "as Status.  Note that only GPIO #1 can\n"
   "be configured as a GPI",

   "Mode Action Strobe\n\n"
   "The GPO is set after receiving an action\n"
   "command.  The GPO pulse occurs Delay\n"
   "milliseconds from the action and is\n"
   "Duration milliseconds in length.",

   "Mode Action Normal\n\n"
   "The GPO is set to either low or high,\n"
   "depending on the value of Polarity, \n"
   "when an action command is received.",

   "Mode Action Pulse\n\n"
   "The GPO is pulsed whenever by an action\n"
   "command. The GPO outputs Number of\n "
   "pulses of Duration milliseconds in\n"
   "length, separated by Interval\n"
   "milliseconds."
};

const char * const PxLEventNames[] =
{
	("Any Event"), // Not used
	("Camera Disconnected"),
	("Hardware Trigger Rising Edge"),
	("Hardware Trigger Falling Edge"),
	("GPI Rising Edge"),
	("GPIFalling Edge"),
	("Hardware Trigger Missed"),
	("PTP Synchronized to Master Clock"),
	("PTP Lost Synchronization from Master Clock"),
    ("Frames Skipped"),
    ("Sensor Scans Synchronized"),
};


/* ---------------------------------------------------------------------------
 * --   Member functions - Public
 * ---------------------------------------------------------------------------
 */
PxLGpio::PxLGpio (GtkBuilder *builder)
: m_gpiLast(false)
{
    //
    // Step 1
    //      Find all of the glade controls

    m_triggerType = new PxLComboBox (GTK_WIDGET( gtk_builder_get_object( builder, "TriggerType_Combo" ) ) );

    m_swTriggerButton = GTK_WIDGET( gtk_builder_get_object( builder, "SwTrigger_Button" ) );

    m_hwTriggerMode = GTK_WIDGET( gtk_builder_get_object( builder, "HwTriggerMode_Combo" ) );
    m_hwTriggePolarity = GTK_WIDGET( gtk_builder_get_object( builder, "HwTriggerPolarity_Combo" ) );
    m_hwTriggerDelay = GTK_WIDGET( gtk_builder_get_object( builder, "HwTriggerDelay_Text" ) );
    m_hwTriggerParam1Type = GTK_WIDGET( gtk_builder_get_object( builder, "TriggerParam1Type_Label" ) );
    m_hwTriggerNumber = GTK_WIDGET( gtk_builder_get_object( builder, "HwTriggerNumber_Text" ) );
    m_hwTriggerUpdate = GTK_WIDGET( gtk_builder_get_object( builder, "HwTriggerUpdate_Button" ) );
    m_hwTriggerDescription = GTK_WIDGET( gtk_builder_get_object( builder, "HardwareTriggerDesc_Label" ) );

    m_gpioNumber = GTK_WIDGET( gtk_builder_get_object( builder, "GpioNumber_Combo" ) );
    m_gpioEnable = GTK_WIDGET( gtk_builder_get_object( builder, "GpioEnable_Checkbox" ) );
    m_gpioMode = new PxLComboBox (GTK_WIDGET( gtk_builder_get_object( builder, "GpioMode_Combo" ) ) );
    m_gpioPolarity = GTK_WIDGET( gtk_builder_get_object( builder, "GpioPolarity_Combo" ) );
    m_gpioParam1Type = GTK_WIDGET( gtk_builder_get_object( builder, "GpioParam1Type_Label" ) );
    m_gpioParam1Value = GTK_WIDGET( gtk_builder_get_object( builder, "GpioParam1Value_Text" ) );
    m_gpioParam1Units = GTK_WIDGET( gtk_builder_get_object( builder, "GpioParam1Units_Label" ) );
    m_gpioParam2Type = GTK_WIDGET( gtk_builder_get_object( builder, "GpioParam2Type_Label" ) );
    m_gpioParam2Value = GTK_WIDGET( gtk_builder_get_object( builder, "GpioParam2Value_Text" ) );
    m_gpioParam2Units = GTK_WIDGET( gtk_builder_get_object( builder, "GpioParam2Units_Label" ) );
    m_gpioParam3Type = GTK_WIDGET( gtk_builder_get_object( builder, "GpioParam3Type_Label" ) );
    m_gpioParam3Value = GTK_WIDGET( gtk_builder_get_object( builder, "GpioParam3Value_Text" ) );
    m_gpioParam3Units = GTK_WIDGET( gtk_builder_get_object( builder, "GpioParam3Units_Label" ) );
    m_gpioUpdate = GTK_WIDGET( gtk_builder_get_object( builder, "GpioUpdate_Button" ) );
    m_gpioDescription = GTK_WIDGET( gtk_builder_get_object( builder, "GpioDesc_Label" ) );

    m_actionCommandType = GTK_WIDGET( gtk_builder_get_object( builder, "ActionCommand_Combo" ) );
    m_actionCommandDelay = GTK_WIDGET( gtk_builder_get_object( builder, "ActionCommandDelay_Text" ) );
    m_actionSendButton = GTK_WIDGET( gtk_builder_get_object( builder, "ActionCommandSend_Button" ) );

    m_events = GTK_WIDGET( gtk_builder_get_object( builder, "Events_Text" ) );
    m_eventsClearButton = GTK_WIDGET( gtk_builder_get_object( builder, "EventsClear_Button" ) );

    m_supportsFrameAction = false;
    m_supportsGpoAction = false;
    m_supportedGpos = NO_GPIOS;

    GtkTextBuffer *buf = gtk_text_view_get_buffer (GTK_TEXT_VIEW (m_events));
    gtk_text_buffer_set_text (buf, "", -1);

}


PxLGpio::~PxLGpio ()
{
    delete m_triggerType;
    delete m_gpioMode;
}

void PxLGpio::refreshRequired (bool noCamera)
{
    if (IsActiveTab (GpioTab))
    {
        if (noCamera)
        {
            // If I am the active tab, then grey out everything
            gdk_threads_add_idle ((GSourceFunc)TriggerDeactivate, this);
            gdk_threads_add_idle ((GSourceFunc)GpioDeactivate, this);
            gdk_threads_add_idle ((GSourceFunc)ActionsDeactivate, this);
            gdk_threads_add_idle ((GSourceFunc)EventsDeactivate, this);
        } else {
            // If I am the active tab, then refresh everything
            gdk_threads_add_idle ((GSourceFunc)TriggerActivate, this);
            gdk_threads_add_idle ((GSourceFunc)GpioActivate, this);
            gdk_threads_add_idle ((GSourceFunc)ActionsActivate, this);
            gdk_threads_add_idle ((GSourceFunc)EventsActivate, this);
        }

        gdk_threads_add_idle ((GSourceFunc)RefreshComplete, this);
        m_numRefreshRequestsOutstanding++;
    } else {
        // If we are not the active tab, only bump the m_numRefreshRequestsOutstanding if there is not
        // one outstanding already; RefreshComplete will be scheduled when the tab becomes active
        if (!m_numRefreshRequestsOutstanding)m_numRefreshRequestsOutstanding++;
    }
}

void PxLGpio::activate()
{
    // I have become the active tab.

    if (gCamera)
    {
        if (m_numRefreshRequestsOutstanding)
        {
            gdk_threads_add_idle ((GSourceFunc)TriggerActivate, this);
            gdk_threads_add_idle ((GSourceFunc)GpioActivate, this);
            gdk_threads_add_idle ((GSourceFunc)ActionsActivate, this);
            gdk_threads_add_idle ((GSourceFunc)EventsActivate, this);
        } else {
            // If GP Input is enabled, start it's poller
            int modeIndex = gGpioTab->m_gpioMode->getSelectedItem();
            bool gpInputEnabled = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON(m_gpioEnable)) &&
                                  m_supportedGpioModes[modeIndex] == GPIO_MODE_INPUT;
            if (gpInputEnabled)
            {
                gCamera->m_poller->pollAdd(gpInputPoll);
            }
        }
    } else {
        gdk_threads_add_idle ((GSourceFunc)TriggerDeactivate, this);
        gdk_threads_add_idle ((GSourceFunc)GpioDeactivate, this);
        gdk_threads_add_idle ((GSourceFunc)ActionsDeactivate, this);
        gdk_threads_add_idle ((GSourceFunc)EventsDeactivate, this);
    }

    m_numRefreshRequestsOutstanding = 1; // As a safety mechanism, tab activation should assert value, it will be set to 0 when RefreshComplete
    gdk_threads_add_idle ((GSourceFunc)RefreshComplete, this);
}

void PxLGpio::deactivate()
{
    // I am no longer the active tab.

    // remove the poller (it's OK if it's not there)
    if (gCamera)
    {
        gCamera->m_poller->pollRemove(gpInputPoll);
    }
}


/* ---------------------------------------------------------------------------
 * --   gtk thread callbacks - used to update controls
 * ---------------------------------------------------------------------------
 */

// Indicate that the refresh is no longer outstanding, it has completed.
static gboolean RefreshComplete (gpointer pData)
{
    PxLGpio *pControls = (PxLGpio *)pData;

    pControls->m_numRefreshRequestsOutstanding--;
    return false;
}

//
// Make trigger controls unselectable
static gboolean TriggerDeactivate (gpointer pData)
{
    PxLGpio *pControls = (PxLGpio *)pData;

    pControls->m_triggerType->setSensitive (false);

    gtk_widget_set_sensitive (pControls->m_swTriggerButton, false);

    gtk_widget_set_sensitive (pControls->m_hwTriggerMode, false);
    gtk_widget_set_sensitive (pControls->m_hwTriggePolarity, false);
    gtk_widget_set_sensitive (pControls->m_hwTriggerDelay, false);
    gtk_widget_set_sensitive (pControls->m_hwTriggerParam1Type, false);
    gtk_widget_set_sensitive (pControls->m_hwTriggerNumber, false);
    gtk_widget_set_sensitive (pControls->m_hwTriggerUpdate, false);
    gtk_label_set_text (GTK_LABEL (pControls->m_hwTriggerDescription), "");

    gtk_widget_set_sensitive (pControls->m_actionSendButton, false);

    pControls->m_supportedHwTriggerModes.clear();

    return false;  //  Only run once....
}

//
// Make image controls selectable (if appropriate)
static gboolean TriggerActivate (gpointer pData)
{
    PxLGpio *pControls = (PxLGpio *)pData;

    vector<int>supportedHwTriggerModes;
    pControls->m_supportsFrameAction = false;

    if (gCamera)
    {
        PXL_RETURN_CODE rc = ApiSuccess;
        PxLTriggerInfo origTrig;

        //
        // Step 0
        //      Clean up old info
        pControls->m_triggerType->removeAll();
        gtk_combo_box_text_remove_all (GTK_COMBO_BOX_TEXT(pControls->m_hwTriggerMode));
        gtk_combo_box_text_remove_all (GTK_COMBO_BOX_TEXT(pControls->m_hwTriggePolarity));

        //
        // Step 1
        //      Figure out the GPIO profile
        int numGpiosSupported = 0;
        float minMode = 0.0, maxMode = 0.0;
        PxLGpio::GpioProfiles gpioProfile = PxLGpio::NO_GPIOS;
        bool lineUsedAsGpio[] = {false, false, false, false};  // true inmdicates the LINE is being used as a GPIO (IE it can't be used for trigger)
        int  lineUsedToTestHardwareMode = TRIGGER_TYPE_INVALID; // indicates that there is no available hardware trigger line.
        rc = gCamera->getGpioRange (&numGpiosSupported, &minMode, &maxMode);
        if (API_SUCCESS(rc) && numGpiosSupported > 0)
            gpioProfile = pControls->GetGpioProfile (numGpiosSupported, maxMode);

        //
        // Step 2
        //      Figure out our Trigger Types.  We do a bit of a cheat here, and we use the GPIO profile
        //      to determine this.
        //         - All cameras that support triggering, support SOFTWARE trigger
        //         - All cameras with a GPIO support HARDWARE trigger
        //         - All PL-X Cameras support ACTION; and all PL-X Cameras are TWO_GPOS_ONE_GPI
        //         - All cameras with FOUR_FLIEXIBLE_GPIOS support a hardware triger on all LINES

        pControls->m_triggerType->addItem (TRIGGER_TYPE_NONE, "None");
        if (gCamera->supported(FEATURE_TRIGGER))
        {
            rc = gCamera->getTriggerValue (origTrig);
            if (API_SUCCESS (rc))
            {

                pControls->m_triggerType->addItem(TRIGGER_TYPE_SOFTWARE, "Software");

                if (gpioProfile != PxLGpio::NO_GPIOS && gpioProfile != PxLGpio::FOUR_FLEXIBLE_GPIOS)
                {
                    pControls->m_triggerType->addItem (TRIGGER_TYPE_HARDWARE, "Hardware");
                    lineUsedToTestHardwareMode =  TRIGGER_TYPE_HARDWARE;                               
                }

                pControls->m_supportsFrameAction = false;
                if (gpioProfile == PxLGpio::TWO_GPOS_ONE_GPI)
                {
                    pControls->m_triggerType->addItem (TRIGGER_TYPE_ACTION,"Action");
                    pControls->m_supportsFrameAction = true;
                } else if (gpioProfile == PxLGpio::FOUR_FLEXIBLE_GPIOS) {
                    //
                    // Step 2a
                    //      We only want to show the lines that are currently not in use as a GPIO


                    PxLGpioInfo gpio;

                    for (int gpioNum=0; gpioNum<4; gpioNum++)
                    {
                        rc = gCamera->getGpioValue (gpioNum, gpio);
                        lineUsedAsGpio [gpioNum] = (API_SUCCESS(rc) && 
                                                   gpio.m_enabled && 
                                                   gpio.m_mode != GPIO_MODE_HARDWARE_TRIGGER);
                    }
                    if (! lineUsedAsGpio[0])
                    {
                        pControls->m_triggerType->addItem (TRIGGER_TYPE_LINE1, "Hardware1");
                        lineUsedToTestHardwareMode = TRIGGER_TYPE_LINE1;
                    }
                    if (! lineUsedAsGpio[1])
                    {
                        pControls->m_triggerType->addItem (TRIGGER_TYPE_LINE2, "Hardware2");
                        lineUsedToTestHardwareMode = TRIGGER_TYPE_LINE2;
                    }
                    if (! lineUsedAsGpio[2])
                    {
                        pControls->m_triggerType->addItem (TRIGGER_TYPE_LINE3, "Hardware3");
                        lineUsedToTestHardwareMode = TRIGGER_TYPE_LINE3;
                    }
                    if (! lineUsedAsGpio[3])
                    {
                        pControls->m_triggerType->addItem (TRIGGER_TYPE_LINE4, "Hardware4");
                        lineUsedToTestHardwareMode = TRIGGER_TYPE_LINE4;
                    }
                }

                //
                // Step 3
                //      Figure out what (hardware) trigger modes the camera supports.  Supported cameas may support mode 0, 1 and 14.
                //      The API will tell us the minimum mode and the maximum mode.  The only 'difficulty' is if the camera supports mode 0
                //      and mode 14 -- how do we know ti if supports mode 1??  The only way to tell, it to try to set it to mode 1 to see if it
                //      works.  

                float minMode = 0.0, maxMode = 0.0;
                float minType = 0.0, maxType = 0.0;
                rc = gCamera->getTriggerRange (&minMode, &maxMode, &minType, &maxType);
                if (API_SUCCESS(rc))
                {
                    supportedHwTriggerModes.push_back((int)minMode);
                    if (minMode == 0.0 && maxMode == 14.0 && lineUsedToTestHardwareMode != TRIGGER_TYPE_INVALID)
                    {
                        // Under these conditions, we may or may not support mode 1.  We have to try it to be sure
                        TEMP_STREAM_STOP();

                        PxLTriggerInfo newTrig = origTrig;
                        newTrig.m_enabled = true;
                        newTrig.m_type = lineUsedToTestHardwareMode;
                        newTrig.m_mode = 1;
                        rc = gCamera->setTriggerValue (newTrig);

                        if (API_SUCCESS(rc)) supportedHwTriggerModes.push_back(1);
                    
                        gCamera->setTriggerValue (origTrig);
                    }
                    if (maxMode == 14.0 &&
                        (find (supportedHwTriggerModes.begin(), supportedHwTriggerModes.end(), 14) == supportedHwTriggerModes.end()))
                    {
                        supportedHwTriggerModes.push_back(14);
                    }
                }
            }
        }

        //
        // Step 4
        //      Set our hardware parameters (if supported, and relevant)
        bool supportsMode0 = find (supportedHwTriggerModes.begin(), supportedHwTriggerModes.end(), 0) != supportedHwTriggerModes.end();
        bool supportsMode1 = find (supportedHwTriggerModes.begin(), supportedHwTriggerModes.end(), 1) != supportedHwTriggerModes.end();
        bool supportsMode14 = find (supportedHwTriggerModes.begin(), supportedHwTriggerModes.end(), 14) != supportedHwTriggerModes.end();
        if (supportsMode0) gtk_combo_box_text_insert_text (GTK_COMBO_BOX_TEXT(pControls->m_hwTriggerMode), PxLGpio::MODE_0, "0");
        if (supportsMode1) gtk_combo_box_text_insert_text (GTK_COMBO_BOX_TEXT(pControls->m_hwTriggerMode), PxLGpio::MODE_1, "1");
        if (supportsMode14) gtk_combo_box_text_insert_text (GTK_COMBO_BOX_TEXT(pControls->m_hwTriggerMode),PxLGpio::MODE_14, "14");
        if (! supportedHwTriggerModes.empty())
        {
            gtk_combo_box_text_insert_text (GTK_COMBO_BOX_TEXT(pControls->m_hwTriggePolarity), POLARITY_NEGATIVE, "Negative");
            gtk_combo_box_text_insert_text (GTK_COMBO_BOX_TEXT(pControls->m_hwTriggePolarity), POLARITY_POSITIVE, "Positive");
        }

        // update the fields to the current trigger setting (or defaults).
        gGpioTab->m_triggerType->makeActive ((int)origTrig.m_type);
        gtk_combo_box_set_active (GTK_COMBO_BOX(gGpioTab->m_hwTriggerMode), (int)origTrig.m_mode);
        UpdateTriggerInfo (origTrig, supportedHwTriggerModes);

    }

    //
    // Step 5
    //      Remember the supported modes for this camera.
    pControls->m_supportedHwTriggerModes = supportedHwTriggerModes;

    return false;  //  Only run once....
}

//
// Make trigger controls unselectable
static gboolean GpioDeactivate (gpointer pData)
{
    PxLGpio *pControls = (PxLGpio *)pData;

    gtk_widget_set_sensitive (pControls->m_gpioNumber, false);
    gtk_widget_set_sensitive (pControls->m_gpioEnable, false);

    pControls->m_gpioMode->setSensitive (false);
    gtk_widget_set_sensitive (pControls->m_gpioPolarity, false);
    gtk_label_set_text (GTK_LABEL (pControls->m_gpioParam1Type), "");
    gtk_widget_set_sensitive (pControls->m_gpioParam1Value, false);
    gtk_label_set_text (GTK_LABEL (pControls->m_gpioParam1Units), "");
    gtk_label_set_text (GTK_LABEL (pControls->m_gpioParam2Type), "");
    gtk_widget_set_sensitive (pControls->m_gpioParam2Value, false);
    gtk_label_set_text (GTK_LABEL (pControls->m_gpioParam2Units), "");
    gtk_label_set_text (GTK_LABEL (pControls->m_gpioParam3Type), "");
    gtk_widget_set_sensitive (pControls->m_gpioParam3Value, false);
    gtk_label_set_text (GTK_LABEL (pControls->m_gpioParam3Units), "");
    gtk_widget_set_sensitive (pControls->m_gpioUpdate, false);
    gtk_label_set_text (GTK_LABEL (pControls->m_gpioDescription), "");

    // Remove the GPI poller (it's OK if there isn't one
    if (gCamera) gCamera->m_poller->pollRemove(gpInputPoll);

    return false;  //  Only run once....
}

//
// Make GPIO controls selectable (if appropriate)
static gboolean GpioActivate (gpointer pData)
{
    PxLGpio *pControls = (PxLGpio *)pData;

    int numGpiosSupported = 0;
    vector<int> supportedModes; 
    pControls->m_supportedGpos = PxLGpio::NO_GPIOS;
    pControls->m_supportsGpoAction = false;

    if (gCamera)
    {
        //
        // Step 0
        //      Clean up old info
        gtk_combo_box_text_remove_all (GTK_COMBO_BOX_TEXT(pControls->m_gpioNumber));
        pControls->m_gpioMode->removeAll();
        gtk_combo_box_text_remove_all (GTK_COMBO_BOX_TEXT(pControls->m_gpioPolarity));

        PXL_RETURN_CODE rc = ApiSuccess;
        PxLGpioInfo origGpio;
        PxLGpioInfo defaultGpio;
        PxLTriggerInfo currentTrig;

        // This will probe a particular GPIO, to see what is and isn't supported.  Assuming GPIOs are supported, it will 
        // usually use the first GPIO to do this (GPIO 0).  However, you do not want to pick a GPIO that is currently being
        // used as a hardware trigger -- that is a special case in that this particualr GPIO is defined as trigger source via
        // FEATURE_TRIGGER, not FEATURE_GPIO.  So, in the event that the first GPIO is being used for a hardware trigger, we
        // will pick a differnt GPIO to probe.
        int gpioToProbe = 0;

        if (gCamera->supported(FEATURE_GPIO))
        {
            float minMode = 0.0, maxMode = 0.0;
            rc = gCamera->getGpioRange (&numGpiosSupported, &minMode, &maxMode);
            if (API_SUCCESS(rc) && numGpiosSupported > 0)
            {
                //
                // Step 1
                //      Figure out the GPIO profile
                pControls->m_supportedGpos = pControls->GetGpioProfile (numGpiosSupported, maxMode);

                //
                // Step 2
                //      Figure out which GPIO I should be using as the 'probe' to see what modes it supports.
                if (pControls->m_supportedGpos == PxLGpio::FOUR_FLEXIBLE_GPIOS)
                {
                    rc = gCamera->getTriggerValue (currentTrig);
                    if (API_SUCCESS(rc))
                    {
                        if (currentTrig.m_enabled &&
                            currentTrig.m_type == TRIGGER_TYPE_LINE1)
                        {
                            gpioToProbe = 1; // Cant sue the first GPIO to probe, so use the second one 
                        }
                    }
                }
                
                //
                // Step 3
                //      Figure out what gpio modes are supported
                bool restoreRequired = false;
                rc = gCamera->getGpioValue (gpioToProbe, origGpio);
                if (API_SUCCESS (rc))
                {
                	//
                    // Step 3a
                    //      We know the camera supports the minMode
                    //  Not so fast... Bugzilla.1277 says that we can't trust this information to be
                    //  true for some cameras.  So, rather than rely on this (potentially incorrect) information,
                    //  we not assume the min is supported
                    //supportedModes.push_back((int)minMode);

                    // Step 3b
                    //      For all of the modes between minMode and maxMode, we simply have to try to
                    //      set them to see if it works
                    // Bugzilla.1277 -- test the min value too
                    //for (float trialMode = minMode+1; trialMode < maxMode; trialMode++)
                    for (float trialMode = minMode; trialMode < maxMode; trialMode++)
                    {
                        if (trialMode == GPIO_MODE_NORMAL)
                        {
                            // This mode is always supported, not need to try
                            supportedModes.push_back((int)trialMode);
                            continue;
                        }
                        // Bugzilla.2582
                        //    Some GPIO profiles always support input
                        if (trialMode == GPIO_MODE_INPUT && (pControls->m_supportedGpos == PxLGpio::TWO_GPOS_ONE_GPI || 
                                                             pControls->m_supportedGpos == PxLGpio::FOUR_FLEXIBLE_GPIOS))
                        {
                            // This mode is always supported, not need to try
                            supportedModes.push_back((int)trialMode);
                            continue;
                        }
                        if (trialMode == GPIO_MODE_HARDWARE_TRIGGER)
                        {
                            if (pControls->m_supportedGpos == PxLGpio::FOUR_FLEXIBLE_GPIOS) supportedModes.push_back((int)trialMode);
                            // This mode is never settable (it's read only)
                            continue;
                        }
                        
                        restoreRequired = true;
                        PxLGpioInfo newGpio = origGpio;
                        newGpio.m_enabled = true;
                        newGpio.m_mode = trialMode;
                        rc = gCamera->setGpioValue (gpioToProbe, newGpio);
                        if (API_SUCCESS (rc))
                        {
                        	supportedModes.push_back((int)trialMode);
                        	// If the camera supports any GPO action, it will support at least the normal action
                        	if (trialMode == GPIO_MODE_ACTION_NORMAL) pControls->m_supportsGpoAction = true;
                        }
                    }

                    //
                    // Step 3c
                    //      We know the camera supports the maxMode
                    if (find (supportedModes.begin(), supportedModes.end(), (int)maxMode) == supportedModes.end())
                    {
                        supportedModes.push_back((int)maxMode);
                    }

                    //
                    // Step 3d
                    //      If we changed it, restore the original gpio value
                    if (restoreRequired) gCamera->setGpioValue(gpioToProbe, origGpio);
                }

                //
                // Step 4
                //      Set GPIO nums.  If we support at least one, pick the GPIO used as the probe as our 'active' one.
                char cActualValue[40];
                for (int i = 0; i<numGpiosSupported; i++)
                {
                    sprintf (cActualValue, "%d", i+1);
                    gtk_combo_box_text_insert_text (GTK_COMBO_BOX_TEXT(pControls->m_gpioNumber),
                                                    i,
                                                    cActualValue);
                }
                gtk_combo_box_set_active (GTK_COMBO_BOX(pControls->m_gpioNumber), gpioToProbe);
                gtk_widget_set_sensitive (pControls->m_gpioNumber, true);

                //
                // Step 5
                //      Populate the supported modes dropdown (with all of the settable ones)
                
                for (vector<int>::iterator it = supportedModes.begin(); it != supportedModes.end(); ++it)
                {
                    if ((*it) == GPIO_MODE_HARDWARE_TRIGGER) continue;  // Don't allow the user to pick this one, it's not writeable
                    pControls->m_gpioMode->addItem (*it, PxLGpioModeStrings[*it]);
                }

                //
                // Step 6
                //      Populate the polarity dropdown
                gtk_combo_box_text_insert_text (GTK_COMBO_BOX_TEXT(pControls->m_gpioPolarity), POLARITY_NEGATIVE, "Negative");
                gtk_combo_box_text_insert_text (GTK_COMBO_BOX_TEXT(pControls->m_gpioPolarity), POLARITY_POSITIVE, "Positive");

                //
                // Step 7
                //      Update the GUI information on the GPO (for GPIO gpioToProbe).
                pControls->m_supportedGpioModes = supportedModes;
                UpdateGpioInfo (origGpio.m_enabled ? origGpio : defaultGpio);

                //
                // Step 8
                //      Updates are only necessary after a user change.
                gtk_widget_set_sensitive (pControls->m_gpioUpdate, false);

                //
                // Step 9
                //      If GP Input is enabled, start it's poller
                if (origGpio.m_enabled && origGpio.m_mode == GPIO_MODE_INPUT)
                {
                    gCamera->m_poller->pollAdd(gpInputPoll);
                }
            }
        }

    }

    //
    // Step 10
    //      Remember the supported modes for this camera.
    pControls->m_supportedGpioModes = supportedModes;

    return false;  //  Only run once....
}

//
// Make Action controls unselectable
static gboolean ActionsDeactivate (gpointer pData)
{
    PxLGpio *pControls = (PxLGpio *)pData;

    gtk_widget_set_sensitive (pControls->m_actionCommandType, false);
    gtk_widget_set_sensitive (pControls->m_actionCommandDelay, false);
    gtk_widget_set_sensitive (pControls->m_actionSendButton, false);

    pControls->m_supportedActions.clear();

    return false;  //  Only run once....
}

//
// Make Action controls selectable (if appropriate)
static gboolean ActionsActivate (gpointer pData)
{
    PxLGpio *pControls = (PxLGpio *)pData;

    bool supportsAnAction = false; // Assume we don't support any

    if (gCamera)
    {
        //
        // Step 0
        //      Clean up old info
        gtk_combo_box_text_remove_all (GTK_COMBO_BOX_TEXT(pControls->m_actionCommandType));

        //
        // Step 1
        //      Set our supported trigger types
        if (pControls->m_supportsFrameAction)
        {
        	gtk_combo_box_text_insert_text (GTK_COMBO_BOX_TEXT(pControls->m_actionCommandType),
        									ACTION_FRAME_TRIGGER,
											"Frame");
        	pControls->m_supportedActions.push_back (ACTION_FRAME_TRIGGER);
        	supportsAnAction = true;
            gtk_combo_box_set_active (GTK_COMBO_BOX(pControls->m_actionCommandType), ACTION_FRAME_TRIGGER);
        }
        if (pControls->m_supportsGpoAction)
        {
        	// We get a little bit cheeky here -- we know that the only cameras that support
        	// GPO actions, have 2 GPOs and a GPI (IE -- a PL-X)
        	if (pControls->m_supportedGpos == PxLGpio::TWO_GPOS_ONE_GPI)
        	{
                gtk_combo_box_text_insert_text (GTK_COMBO_BOX_TEXT(pControls->m_actionCommandType),
                								ACTION_GPO1,
                                                "GPO 1");
                pControls->m_supportedActions.push_back (ACTION_GPO1);
                gtk_combo_box_text_insert_text (GTK_COMBO_BOX_TEXT(pControls->m_actionCommandType),
                								ACTION_GPO2,
                                                "GPO 2");
                pControls->m_supportedActions.push_back (ACTION_GPO2);
                if (! supportsAnAction)
                {
                	gtk_combo_box_set_active (GTK_COMBO_BOX(pControls->m_actionCommandType), ACTION_GPO1);
                }
                supportsAnAction = true;
        	}
        }

        // Add the SensorSync action as well, if this camera supports ActionTrigger.
        // This may sound a little odd, as we don't know if this specific camera supports
        // SensorSync -- but recall that actions are sent on the network to ALL cameras;
        // only cameras that support it will take action; others will ignore it.
        if (pControls->m_supportsFrameAction)
        {
            gtk_combo_box_text_insert_text (GTK_COMBO_BOX_TEXT(pControls->m_actionCommandType),
                                            ACTION_SENSOR_SYNC,
                                            "SensorSync");
            pControls->m_supportedActions.push_back (ACTION_SENSOR_SYNC);
            supportsAnAction = true;
        }

        gtk_widget_set_sensitive (pControls->m_actionCommandType, supportsAnAction);

        //
        // Step 2
        //		Set the default delay
        gtk_entry_set_text (GTK_ENTRY (pControls->m_actionCommandDelay), "0.0");
        gtk_widget_set_sensitive (pControls->m_actionCommandDelay, supportsAnAction);

        //
        // Step 3
        //		Enable the button (if we support actions
        gtk_widget_set_sensitive (pControls->m_actionSendButton, supportsAnAction);

    }

    return false;  //  Only run once....
}

//
// Make Events controls unselectable
static gboolean EventsDeactivate (gpointer pData)
{
    PxLGpio *pControls = (PxLGpio *)pData;

    gtk_widget_set_sensitive (pControls->m_events, false);
    gtk_widget_set_sensitive (pControls->m_eventsClearButton, false);

    //Uncomment if we want to clear old events when the camera changes
    //GtkTextBuffer *buf = gtk_text_view_get_buffer (GTK_TEXT_VIEW (pControls->m_events));
    //gtk_test_buffer_set_text (buf, " ", -1);

    // Cancel all event notifications for this camera
    if (gCamera)
    {
    	for (int i = EVENT_CAMERA_DISCONNECTED+1; i <= EVENT_LAST; i++)
    	{
    		gCamera->setEventCallback(i, NULL, NULL);
    	}
    }

    return false;  //  Only run once....
}

//
// Make Event controls selectable (if appropriate)
static gboolean EventsActivate (gpointer pData)
{
    PxLGpio *pControls = (PxLGpio *)pData;


    if (gCamera)
    {
        PXL_RETURN_CODE rc = ApiSuccess;
        bool eventsSupported = false;

        // Register our event handler for all supported events
        for (int i = EVENT_CAMERA_DISCONNECTED+1; i <= EVENT_LAST; i++)
        {
        	rc = gCamera->setEventCallback (i, pControls, PxLEventCallback);
        	if (API_SUCCESS(rc)) eventsSupported = true;
        }

        gtk_widget_set_sensitive (pControls->m_events, eventsSupported);
        gtk_widget_set_sensitive (pControls->m_eventsClearButton, eventsSupported);
    }

    return false;  //  Only run once....
}

//
// Called periodically when general purpose inputs -- reads the current value
PXL_RETURN_CODE GetCurrentGpio()
{
    PXL_RETURN_CODE rc = ApiSuccess;

    PxLAutoLock lock(&gCameraLock);
    if (gCamera && gGpioTab)
    {
        // It's safe to assume the camera supports GPIO, as this function will not be called
        // otherwise.  If we were to check via pCamera->supported (FEATURE_GPIO) or
        // pCamera->continuousSupported (FEATURE_GPIO), then that will perform a PxLGetCameraFeatures,
        // which is a lot of work for not.
        PxLGpioInfo currentGpio;
        int requestedGpioNum = gtk_combo_box_get_active (GTK_COMBO_BOX(gGpioTab->m_gpioNumber));  // This is '0' based
        rc = gCamera->getGpioValue(requestedGpioNum, currentGpio);
        if (API_SUCCESS(rc)) gGpioTab->m_gpiLast = currentGpio.m_param1 == 1.0f;
    }

    return rc;
}

//
// Called periodically when doing continuous exposure updates -- updates the user controls
void UpdateGpiStatus()
{
    if (gCamera && gGpioTab)
    {
        PxLAutoLock lock(&gCameraLock);

        gtk_entry_set_text (GTK_ENTRY (gGpioTab->m_gpioParam1Value),
                                       gGpioTab->m_gpiLast ? "Signaled" : "Not signaled");
    }
}

static void UpdateTriggerInfo (PxLTriggerInfo& info, vector<int>& supportedHwTriggerModes)
{
    // figure out if a trigger mode is active, so that we can activate the appropriate set of controls.
    bool inSoftwareTriggerMode = info.m_enabled && info.m_type ==  TRIGGER_TYPE_SOFTWARE;
    bool inHardwareTriggerMode = info.m_enabled && IS_HARDWARE_TRIGGER (info.m_type);

    gGpioTab->m_triggerType->makeActive (info.m_enabled ? (int)info.m_type : TRIGGER_TYPE_NONE);
    gGpioTab->m_triggerType->setSensitive (true);

    gtk_widget_set_sensitive (gGpioTab->m_swTriggerButton, inSoftwareTriggerMode);

    if (!supportedHwTriggerModes.empty())
    {
        gtk_combo_box_set_active (GTK_COMBO_BOX(gGpioTab->m_hwTriggerMode), gGpioTab->ModeToIndex(info.m_mode));
        gtk_widget_set_sensitive (gGpioTab->m_hwTriggerMode, inHardwareTriggerMode);

        gtk_combo_box_set_active (GTK_COMBO_BOX(gGpioTab->m_hwTriggePolarity), (int)info.m_polarity);
        gtk_widget_set_sensitive (gGpioTab->m_hwTriggePolarity, inHardwareTriggerMode);

        char cActualValue[40];
        sprintf (cActualValue, "%8.1f", info.m_delay * 1000.0);
        gtk_entry_set_text (GTK_ENTRY (gGpioTab->m_hwTriggerDelay), cActualValue);
        gtk_widget_set_sensitive (gGpioTab->m_hwTriggerDelay, inHardwareTriggerMode);

        bool supportsMode14 = find (supportedHwTriggerModes.begin(), supportedHwTriggerModes.end(), 14) != supportedHwTriggerModes.end();
        if (supportsMode14 && info.m_mode == 14)
        {
            gtk_label_set_text (GTK_LABEL (gGpioTab->m_hwTriggerParam1Type), "Number: ");
            sprintf (cActualValue, "%d", (int)info.m_number);
            gtk_entry_set_text (GTK_ENTRY (gGpioTab->m_hwTriggerNumber), cActualValue);
            gtk_widget_set_sensitive (gGpioTab->m_hwTriggerNumber, inHardwareTriggerMode);
        } else {
            gtk_label_set_text (GTK_LABEL (gGpioTab->m_hwTriggerParam1Type), "");
            gtk_widget_set_sensitive (gGpioTab->m_hwTriggerNumber, false);
        }

        int descriptionIndex = max ((int)info.m_mode, 0);
        if (descriptionIndex >= (int)(sizeof(PxLTriggerModeDescriptions) / sizeof (PxLTriggerModeDescriptions[0])))
        {
            descriptionIndex = (int)(sizeof(PxLTriggerModeDescriptions) / sizeof (PxLTriggerModeDescriptions[0])) - 1;
        }
        gtk_label_set_text (GTK_LABEL (gGpioTab->m_hwTriggerDescription), PxLTriggerModeDescriptions[descriptionIndex]);
    }

}

static void UpdateGpioInfo (PxLGpioInfo& info) // Be sure m_supportedGpioModes is current
{
    // If the GPIO is enabled as a hardware trigger, then the GPIO is read only as this configuration
    // is controlled via FEATURE_TRIGGER, not FEATURE_GPIO
    bool gpioReadOnly = info.m_enabled && info.m_mode == GPIO_MODE_HARDWARE_TRIGGER;
    
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(gGpioTab->m_gpioEnable), info.m_enabled);
    gtk_widget_set_sensitive (gGpioTab->m_gpioEnable, ! gpioReadOnly);

    if (gpioReadOnly)
    {
        gGpioTab->m_gpioMode->makeActive (GPIO_MODE_HARDWARE_TRIGGER);
        gGpioTab->m_gpioMode->setSensitive (false);        
    } else {
        int index = max ((int)gGpioTab->m_supportedGpioModes.size()-1, 0);
        for (; index > 0; index--) if (gGpioTab->m_supportedGpioModes[index] == (int)info.m_mode) break;
        gGpioTab->m_gpioMode->makeActive (index);
        gGpioTab->m_gpioMode->setSensitive (true);
    }

    gtk_combo_box_set_active (GTK_COMBO_BOX(gGpioTab->m_gpioPolarity), (int)info.m_polarity);
    gtk_widget_set_sensitive (gGpioTab->m_gpioPolarity, ! gpioReadOnly);

    // Start with all of the optional parameters in a NULL state.  They will be
    // set properly below
    gtk_label_set_text (GTK_LABEL (gGpioTab->m_gpioParam1Type), "");
    gtk_entry_set_text (GTK_ENTRY (gGpioTab->m_gpioParam1Value), "");
    gtk_label_set_text (GTK_LABEL (gGpioTab->m_gpioParam1Units), "");
    gtk_widget_set_sensitive (gGpioTab->m_gpioParam1Value, false);

    gtk_label_set_text (GTK_LABEL (gGpioTab->m_gpioParam2Type), "");
    gtk_entry_set_text (GTK_ENTRY (gGpioTab->m_gpioParam2Value), "");
    gtk_label_set_text (GTK_LABEL (gGpioTab->m_gpioParam2Units), "");
    gtk_widget_set_sensitive (gGpioTab->m_gpioParam2Value, false);

    gtk_label_set_text (GTK_LABEL (gGpioTab->m_gpioParam3Type), "");
    gtk_entry_set_text (GTK_ENTRY (gGpioTab->m_gpioParam3Value), "");
    gtk_label_set_text (GTK_LABEL (gGpioTab->m_gpioParam3Units), "");
    gtk_widget_set_sensitive (gGpioTab->m_gpioParam3Value, false);

    char cActualValue[40];
    switch ((int)info.m_mode)
    {
    // BE sure to convert the times to milliseconds
    case GPIO_MODE_STROBE:
        gtk_label_set_text (GTK_LABEL (gGpioTab->m_gpioParam1Type), "Delay");
        sprintf (cActualValue, "%8.1f", info.m_param1 * 1000.0f);
        gtk_entry_set_text (GTK_ENTRY (gGpioTab->m_gpioParam1Value), cActualValue);
        gtk_label_set_text (GTK_LABEL (gGpioTab->m_gpioParam1Units), "milliseconds");
        gtk_widget_set_sensitive (gGpioTab->m_gpioParam1Value, true);

        gtk_label_set_text (GTK_LABEL (gGpioTab->m_gpioParam2Type), "Duration");
        sprintf (cActualValue, "%8.1f", info.m_param2 * 1000.0f);
        gtk_entry_set_text (GTK_ENTRY (gGpioTab->m_gpioParam2Value), cActualValue);
        gtk_label_set_text (GTK_LABEL (gGpioTab->m_gpioParam2Units), "milliseconds");
        gtk_widget_set_sensitive (gGpioTab->m_gpioParam2Value, true);

        break;
    case GPIO_MODE_PULSE:
        gtk_label_set_text (GTK_LABEL (gGpioTab->m_gpioParam1Type), "Number");
        sprintf (cActualValue, "%d", (int)info.m_param1);
        gtk_entry_set_text (GTK_ENTRY (gGpioTab->m_gpioParam1Value), cActualValue);
        gtk_label_set_text (GTK_LABEL (gGpioTab->m_gpioParam1Units), "");
        gtk_widget_set_sensitive (gGpioTab->m_gpioParam1Value, true);

        gtk_label_set_text (GTK_LABEL (gGpioTab->m_gpioParam2Type), "Duration");
        sprintf (cActualValue, "%8.1f", info.m_param2 * 1000.0f);
        gtk_entry_set_text (GTK_ENTRY (gGpioTab->m_gpioParam2Value), cActualValue);
        gtk_label_set_text (GTK_LABEL (gGpioTab->m_gpioParam2Units), "milliseconds");
        gtk_widget_set_sensitive (gGpioTab->m_gpioParam2Value, true);

        gtk_label_set_text (GTK_LABEL (gGpioTab->m_gpioParam3Type), "Interval");
        sprintf (cActualValue, "%8.1f", info.m_param3 * 1000.0f);
        gtk_entry_set_text (GTK_ENTRY (gGpioTab->m_gpioParam3Value), cActualValue);
        gtk_label_set_text (GTK_LABEL (gGpioTab->m_gpioParam3Units), "milliseconds");
        gtk_widget_set_sensitive (gGpioTab->m_gpioParam3Value, true);
        break;
    case GPIO_MODE_INPUT:
        gtk_label_set_text (GTK_LABEL (gGpioTab->m_gpioParam1Type), "Status");
        gtk_entry_set_text (GTK_ENTRY (gGpioTab->m_gpioParam1Value),
                                       info.m_param1 == 0 ? "Not signaled" : "Signaled");
        gtk_label_set_text (GTK_LABEL (gGpioTab->m_gpioParam1Units), "");
        gtk_widget_set_sensitive (gGpioTab->m_gpioParam1Value, false);

        break;
    // All others....
    default:
        break;
    }

    gtk_label_set_text (GTK_LABEL (gGpioTab->m_gpioDescription), PxLGpioModeDescriptions[(int)info.m_mode]);
}



/* ---------------------------------------------------------------------------
 * --   Control functions from the Glade project
 * ---------------------------------------------------------------------------
 */

extern "C" void NewTriggerSelected
    (GtkWidget* widget, GdkEventExpose* event, gpointer userdata )
{
    if (! gCamera || ! gCamera) return;
    if (gGpioTab->m_numRefreshRequestsOutstanding) return;

    //
    // Step 1
    //      Determine the current trigger mode.
    PxLTriggerInfo actualTrigger;
    gCamera->getTriggerValue(actualTrigger);

    //
    // Step 2
    //      Determine the type of trigger the user wants.
    int requestedTriggerType = gGpioTab->m_triggerType->getSelectedItem();
    PxLTriggerInfo requestedTrigger;

    int userModeIndex;
    requestedTrigger.m_enabled = true;
    requestedTrigger.m_type = requestedTriggerType;
    switch (requestedTriggerType)
    {
    case TRIGGER_TYPE_SOFTWARE:
        requestedTrigger.m_mode = 0.0;
        requestedTrigger.m_delay = 0.0;
        requestedTrigger.m_number = 1.0;
        break;
    case TRIGGER_TYPE_HARDWARE:
    case TRIGGER_TYPE_ACTION:
    case TRIGGER_TYPE_LINE2:
    case TRIGGER_TYPE_LINE3:
    case TRIGGER_TYPE_LINE4:
        userModeIndex = gtk_combo_box_get_active (GTK_COMBO_BOX(gGpioTab->m_hwTriggerMode));
        requestedTrigger.m_mode = gGpioTab->m_supportedHwTriggerModes[userModeIndex];
        requestedTrigger.m_polarity = (float)gtk_combo_box_get_active (GTK_COMBO_BOX(gGpioTab->m_hwTriggePolarity));
        requestedTrigger.m_delay = atof (gtk_entry_get_text (GTK_ENTRY (gGpioTab->m_hwTriggerDelay))) / 1000.0f;
        requestedTrigger.m_number = atof (gtk_entry_get_text (GTK_ENTRY (gGpioTab->m_hwTriggerNumber)));
        break;
    case TRIGGER_TYPE_NONE:
    default:
        requestedTrigger.m_enabled = false;
        break;
    }

    //
    // Step 3
    //      Attempt to set the trigger
    PxLAutoLock lock(&gCameraLock);
    PXL_RETURN_CODE rc;
    {
        TEMP_STREAM_STOP();

        rc = gCamera->setTriggerValue(requestedTrigger);

        if (!API_SUCCESS (rc))
        {
            //
            // Step 4
            //      If the set didn't work, report the error and then refresh the trigger controls
            // Pop up an error message
            GtkWidget *popupError = gtk_message_dialog_new (gTopLevelWindow,
                                                 GTK_DIALOG_DESTROY_WITH_PARENT,
                                                 GTK_MESSAGE_ERROR,
                                                 GTK_BUTTONS_CLOSE,
                                                 "Setting trigger returned error code - 0x%x", rc);
            gtk_dialog_run (GTK_DIALOG (popupError));  // This makes the popup modal
            gtk_widget_destroy (popupError);

            rc = gCamera->getTriggerValue(actualTrigger);
        } else {
            //
            // Step 5
            //      If the user is enabling or disabling a hardware trigger, then we need to update the GPIO information too
            if (gGpioTab->m_supportedGpos == PxLGpio::FOUR_FLEXIBLE_GPIOS)
            {
                bool bSelectedGpioBecomingHwTrigger = false;
                bool bSelectedGpioLosingHwTrigger = false;
                
                int selectedGpioNum = gtk_combo_box_get_active (GTK_COMBO_BOX(gGpioTab->m_gpioNumber));  // This is '0' based
                
                if ((requestedTriggerType == TRIGGER_TYPE_LINE1 && selectedGpioNum == 0)) bSelectedGpioBecomingHwTrigger = true;
                else if ((requestedTriggerType == TRIGGER_TYPE_LINE2 && selectedGpioNum == 1)) bSelectedGpioBecomingHwTrigger = true;
                else if ((requestedTriggerType == TRIGGER_TYPE_LINE3 && selectedGpioNum == 2)) bSelectedGpioBecomingHwTrigger = true;
                else if ((requestedTriggerType == TRIGGER_TYPE_LINE4 && selectedGpioNum == 3)) bSelectedGpioBecomingHwTrigger = true;
                if ((actualTrigger.m_type == TRIGGER_TYPE_LINE1 && selectedGpioNum == 0)) bSelectedGpioLosingHwTrigger = true;
                else if ((actualTrigger.m_type == TRIGGER_TYPE_LINE2 && selectedGpioNum == 1)) bSelectedGpioLosingHwTrigger = true;
                else if ((actualTrigger.m_type == TRIGGER_TYPE_LINE3 && selectedGpioNum == 2)) bSelectedGpioLosingHwTrigger = true;
                else if ((actualTrigger.m_type == TRIGGER_TYPE_LINE4 && selectedGpioNum == 3)) bSelectedGpioLosingHwTrigger = true;

                if (bSelectedGpioBecomingHwTrigger || bSelectedGpioLosingHwTrigger)
                {
                    // We need to update the displayed GPIO info, because it has hanged.  the easiest way to do this is to change
                    // the GPIO being shown, to a differnt one
                    gtk_combo_box_set_active (GTK_COMBO_BOX(gGpioTab->m_gpioNumber), selectedGpioNum == 0 ? 1 : 0);
                }
            }
            
            actualTrigger = requestedTrigger;
        }
    }

    //
    // Step 5
    //      Update the appropriate set of controls to match the trigger type.
    UpdateTriggerInfo (actualTrigger, gGpioTab->m_supportedHwTriggerModes);

    // Update other tabs the next time they are activated
    gStreamTab->refreshRequired(false);
}

extern "C" void SwTriggerButtonPressed
    (GtkWidget* widget, GdkEventExpose* event, gpointer userdata )
{
    if (! gCamera || ! gCamera) return;
    if (gGpioTab->m_numRefreshRequestsOutstanding) return;

    PxLAutoLock lock(&gCameraLock);

    // simply capture a throw away frame.
    std::vector<U8> frameBuf (gCamera->imageSizeInBytes());
    FRAME_DESC     frameDesc;

    gCamera->getNextFrame (frameBuf.size(), &frameBuf[0], &frameDesc);
}

extern "C" void ActionCommandButtonPressed
    (GtkWidget* widget, GdkEventExpose* event, gpointer userdata )
{
    if (! gCamera || ! gCamera) return;
    if (gGpioTab->m_numRefreshRequestsOutstanding) return;

    PxLAutoLock lock(&gCameraLock);

    int   actionCommand = gGpioTab->m_supportedActions[gtk_combo_box_get_active (GTK_COMBO_BOX(gGpioTab->m_actionCommandType))];
    double actionDelay = atof (gtk_entry_get_text (GTK_ENTRY(gGpioTab->m_actionCommandDelay)));

    gCamera->sendActionCommand (actionCommand, actionDelay);
}

extern "C" void NewTriggerModeSelected
    (GtkWidget* widget, GdkEventExpose* event, gpointer userdata )
{
    if (! gCamera || ! gCamera) return;
    if (gGpioTab->m_numRefreshRequestsOutstanding) return;

    int triggerType = gGpioTab->m_triggerType->getSelectedItem();
    bool hwTriggering = IS_HARDWARE_TRIGGER ((float)triggerType);
    PxLGpio::HW_TRIGGER_MODES triggerMode = (PxLGpio::HW_TRIGGER_MODES)gtk_combo_box_get_active (GTK_COMBO_BOX(gGpioTab->m_hwTriggerMode));
    gtk_label_set_text (GTK_LABEL (gGpioTab->m_hwTriggerDescription), PxLTriggerModeDescriptions[triggerMode]);

    if (triggerMode == PxLGpio::MODE_14)
    {
        gtk_label_set_text (GTK_LABEL (gGpioTab->m_hwTriggerParam1Type), "Number: ");
        char cActualValue[40];
        sprintf (cActualValue, "%d", 1);  // default to just one frame
        gtk_entry_set_text (GTK_ENTRY (gGpioTab->m_hwTriggerNumber), cActualValue);
        gtk_widget_set_sensitive (gGpioTab->m_hwTriggerNumber, true);
    } else {
        gtk_label_set_text (GTK_LABEL (gGpioTab->m_hwTriggerParam1Type), "");
        gtk_widget_set_sensitive (gGpioTab->m_hwTriggerNumber, false);
    }

    gtk_widget_set_sensitive (gGpioTab->m_hwTriggerUpdate, hwTriggering);
}

extern "C" void TriggerParamChanged
    (GtkWidget* widget, GdkEventExpose* event, gpointer userdata )
{
    if (! gCamera || ! gCamera) return;
    if (gGpioTab->m_numRefreshRequestsOutstanding) return;

    int triggerType = gGpioTab->m_triggerType->getSelectedItem();
    bool hwTriggering =  IS_HARDWARE_TRIGGER ((float)triggerType);

    gtk_widget_set_sensitive (gGpioTab->m_hwTriggerUpdate, hwTriggering);
}

extern "C" void TriggerUpdateButtonPressed
    (GtkWidget* widget, GdkEventExpose* event, gpointer userdata )
{
    if (! gCamera || ! gCamera) return;
    if (gGpioTab->m_numRefreshRequestsOutstanding) return;

    PxLTriggerInfo requestedTrigger;
    PxLAutoLock lock(&gCameraLock);
    PXL_RETURN_CODE rc;
    {
        requestedTrigger.m_enabled = true;
        requestedTrigger.m_type = gGpioTab->m_triggerType->getSelectedItem();
        int userModeIndex = gtk_combo_box_get_active (GTK_COMBO_BOX(gGpioTab->m_hwTriggerMode));
        requestedTrigger.m_mode = gGpioTab->m_supportedHwTriggerModes[userModeIndex];
        requestedTrigger.m_polarity = gtk_combo_box_get_active (GTK_COMBO_BOX(gGpioTab->m_hwTriggePolarity));
        requestedTrigger.m_delay = atof (gtk_entry_get_text (GTK_ENTRY (gGpioTab->m_hwTriggerDelay))) / 1000.0f;
        requestedTrigger.m_number = atof (gtk_entry_get_text (GTK_ENTRY (gGpioTab->m_hwTriggerNumber)));
        TEMP_STREAM_STOP();
        rc = gCamera->setTriggerValue(requestedTrigger);

        if (!API_SUCCESS (rc))
        {
            GtkWidget *popupError = gtk_message_dialog_new (gTopLevelWindow,
                                                 GTK_DIALOG_DESTROY_WITH_PARENT,
                                                 GTK_MESSAGE_ERROR,
                                                 GTK_BUTTONS_CLOSE,
                                                 "Setting trigger returned error code - 0x%x", rc);
            gtk_dialog_run (GTK_DIALOG (popupError));  // This makes the popup modal
            gtk_widget_destroy (popupError);

            PxLTriggerInfo actualTrigger;
            rc = gCamera->getTriggerValue(actualTrigger);
            if (API_SUCCESS(rc)) UpdateTriggerInfo (actualTrigger, gGpioTab->m_supportedHwTriggerModes);
        }
    }

    gtk_widget_set_sensitive (gGpioTab->m_hwTriggerUpdate, false);

    // Update other tabs the next time they are activated
    gStreamTab->refreshRequired(false);
}

extern "C" void NewGpioNumSelected
    (GtkWidget* widget, GdkEventExpose* event, gpointer userdata )
{
    if (! gCamera || ! gCamera) return;
    if (gGpioTab->m_numRefreshRequestsOutstanding) return;

    int requestedGpioNum = gtk_combo_box_get_active (GTK_COMBO_BOX(gGpioTab->m_gpioNumber));  // This is '0' based
    PxLGpioInfo requestedGpio;

    PxLAutoLock lock(&gCameraLock);
    PXL_RETURN_CODE rc;
    {
    	rc = gCamera->getGpioValue(requestedGpioNum, requestedGpio);

        if (API_SUCCESS (rc))
        {
            //If the new GPO we just selected is disabled, then we don't actually know the mode (it's impossible
            // to tell.  So, in these circumstances, use the default mode of GPIO_MODE_NORMAL.
            // Bugzilla.2139 -
            //    Also, if the GPIO is in a differnt enabled/disabled state as the former GPIO, we don't
            //    want the application to confuse this for a user change to the enable/disable.  We use
            //    m_numRefresRequestsOutstanding to prevent this.
            gGpioTab->m_numRefreshRequestsOutstanding++; // We don't want any controls to trigger a change while we are updating them
            gdk_threads_add_idle ((GSourceFunc)RefreshComplete, gGpioTab);
            if (! requestedGpio.m_enabled) requestedGpio.m_mode = GPIO_MODE_NORMAL;
            UpdateGpioInfo (requestedGpio);
            
            // Bugzilla.2582
            //     Update the GPIO modes to just include the modes for this specific GPIO.
            vector<int> supportedModes = gGpioTab->m_supportedGpioModes;
            if (requestedGpioNum == 0 && (gGpioTab->m_supportedGpos == PxLGpio::TWO_GPOS_ONE_GPI))
            {
               // remove (supportedModes, GPIO_MODE_INPUT);
               supportedModes.erase (remove (supportedModes.begin(), supportedModes.end(), GPIO_MODE_INPUT), supportedModes.end()); // These cameras have dedicated GPIs, GPIO#1 is NEVER a GPI
            } else if (requestedGpioNum == 1 && gGpioTab->m_supportedGpos != PxLGpio::FOUR_FLEXIBLE_GPIOS) {
               // remove (supportedModes, GPIO_MODE_INPUT);
               supportedModes.erase (remove (supportedModes.begin(), supportedModes.end(), GPIO_MODE_INPUT), supportedModes.end()); // GPIO#2 is NEVER a GPI
            } else if ((requestedGpioNum == 2 && gGpioTab->m_supportedGpos == PxLGpio::TWO_GPOS_ONE_GPI)) {
               // these are the dedicated GPIs on these cameras; only offer the choice of GPI
               supportedModes.clear();
               supportedModes.push_back (GPIO_MODE_INPUT);
            }
            
            gGpioTab->m_gpioMode->removeAll ();
            // If the GPIO the user selected IS the one used for HE Trigger, then the HE Trigger will be shown as the 
            // mode, and it will be read only.  If the GPIO the user selected is NOT HW Trigger, then show all supported
            // modes other than hardare trigger -- as the user cannot select this one (it's controlled via FEATURE_TRIGGEr not 
            // FEATURE_GPIO)
            bool userSelectedGpioIsHwTrigger = requestedGpio.m_enabled && requestedGpio.m_mode == GPIO_MODE_HARDWARE_TRIGGER;
            if (userSelectedGpioIsHwTrigger)
            {
                gGpioTab->m_gpioMode->addItem (GPIO_MODE_HARDWARE_TRIGGER, PxLGpioModeStrings[GPIO_MODE_HARDWARE_TRIGGER]);

            } else {
                for (vector<int>::iterator it = supportedModes.begin(); it != supportedModes.end(); ++it)
                {
                    if (*it == GPIO_MODE_HARDWARE_TRIGGER) continue;  // Don't give this as a user selectable option
                    gGpioTab->m_gpioMode->addItem (*it, PxLGpioModeStrings[*it]);
                }
            }

            //  If the GPIO is enabled, then show the correct mode and polarity
            if (requestedGpio.m_enabled) 
            {
                gGpioTab->m_gpioMode->makeActive (requestedGpio.m_mode);
                gtk_combo_box_set_active (GTK_COMBO_BOX(gGpioTab->m_gpioPolarity), requestedGpio.m_polarity);                
            }

            // If the GPIO is HwTrigger, then the Enable, Mode, and Polarity are read only.  otherwise they are read/write
            gtk_widget_set_sensitive (gGpioTab->m_gpioEnable, !userSelectedGpioIsHwTrigger);
            gGpioTab->m_gpioMode->setSensitive (!userSelectedGpioIsHwTrigger);
            gtk_widget_set_sensitive (gGpioTab->m_gpioPolarity, !userSelectedGpioIsHwTrigger);
            
            
            // If GP Input is enabled, start it's poller
            if (requestedGpio.m_enabled && requestedGpio.m_mode == GPIO_MODE_INPUT)
            {
                // This won't add it if it's already there
                gCamera->m_poller->pollAdd(gpInputPoll);
            } else {
                // This will safely do nothing if there is no poller
                gCamera->m_poller->pollRemove(gpInputPoll);
            }
        }
    }
}

extern "C" void GpioEnableToggled
    (GtkWidget* widget, GdkEventExpose* event, gpointer userdata )
{
    if (! gCamera || ! gCamera) return;
    if (gGpioTab->m_numRefreshRequestsOutstanding) return;

    bool gpioEnable = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON(gGpioTab->m_gpioEnable));
    int requestedGpioNum = gtk_combo_box_get_active (GTK_COMBO_BOX(gGpioTab->m_gpioNumber));  // This is '0' based

    PxLGpioInfo requestedGpio;

    requestedGpio.m_enabled = gpioEnable;
    int modeIndex = gGpioTab->m_gpioMode->getSelectedItem();
    requestedGpio.m_mode = (float)gGpioTab->m_supportedGpioModes[modeIndex];
    requestedGpio.m_polarity = gtk_combo_box_get_active (GTK_COMBO_BOX(gGpioTab->m_gpioPolarity));
    // Set the optional parameters.  The lack of breaks is intentional, as they are sequenced accordingly
    switch ((int)requestedGpio.m_mode)
    {
    // Be sure to convert the time quantities from milliseconds to seconds
    case GPIO_MODE_PULSE:
        requestedGpio.m_param3 = atof (gtk_entry_get_text (GTK_ENTRY (gGpioTab->m_gpioParam3Value))) / 1000.0f;
    case GPIO_MODE_STROBE:
        requestedGpio.m_param2 = atof (gtk_entry_get_text (GTK_ENTRY (gGpioTab->m_gpioParam2Value))) / 1000.0f;
        requestedGpio.m_param1 = atof (gtk_entry_get_text (GTK_ENTRY (gGpioTab->m_gpioParam1Value)));
        if ((int)requestedGpio.m_mode == GPIO_MODE_STROBE) requestedGpio.m_param1 /= 1000.0f;
    default:
        break;
    }

    PxLAutoLock lock(&gCameraLock);
    PXL_RETURN_CODE setRc;
    {
        TEMP_STREAM_STOP();

        setRc = gCamera->setGpioValue(requestedGpioNum, requestedGpio);

        PxLGpioInfo actualGpio;
        gGpioTab->m_numRefreshRequestsOutstanding++; // We don't want any controls to trigger a change while we are updating them
        gdk_threads_add_idle ((GSourceFunc)RefreshComplete, gGpioTab);

        actualGpio = requestedGpio;
        gCamera->getGpioValue(requestedGpioNum, actualGpio);
        UpdateGpioInfo (actualGpio);

        if (!API_SUCCESS (setRc))
        {
            GtkWidget *popupError = gtk_message_dialog_new (gTopLevelWindow,
                                                 GTK_DIALOG_DESTROY_WITH_PARENT,
                                                 GTK_MESSAGE_ERROR,
                                                 GTK_BUTTONS_CLOSE,
                                                 "%s GPIO returned error code - 0x%x",
                                                     gpioEnable ? "Enabling" : "Disabling",
                                                     setRc);
            gtk_dialog_run (GTK_DIALOG (popupError));  // This makes the popup modal
            gtk_widget_destroy (popupError);

        }

        // Now consider the impact on cameras with 4 flexible GPIOs.  As GPIOS are enabled and disabled, the set of lines
        // that can be used as a hardware trigger can change.  So update the supported trigger modes as appropriate.
        if (gGpioTab->m_supportedGpos == PxLGpio::FOUR_FLEXIBLE_GPIOS)
        {
            if (gpioEnable)
            {
                // Will only delete if it's there
                if (requestedGpioNum == 0) gGpioTab->m_triggerType->removeItem (TRIGGER_TYPE_LINE1);
                else if (requestedGpioNum == 1) gGpioTab->m_triggerType->removeItem (TRIGGER_TYPE_LINE2);
                else if (requestedGpioNum == 2) gGpioTab->m_triggerType->removeItem (TRIGGER_TYPE_LINE3);
                else if (requestedGpioNum == 3) gGpioTab->m_triggerType->removeItem (TRIGGER_TYPE_LINE4);
            } else {
                // Will only add it if it's not there
                if (requestedGpioNum == 0) gGpioTab->m_triggerType->addItem (TRIGGER_TYPE_LINE1, "Hardware1");
                else if (requestedGpioNum == 1) gGpioTab->m_triggerType->addItem (TRIGGER_TYPE_LINE2, "Hardware2");
                else if (requestedGpioNum == 2) gGpioTab->m_triggerType->addItem (TRIGGER_TYPE_LINE3, "Hardware3");
                else if (requestedGpioNum == 3) gGpioTab->m_triggerType->addItem (TRIGGER_TYPE_LINE4, "Hardware4");
            }
        }

        // If GP Input is enabled, start it's poller
        if (actualGpio.m_enabled && actualGpio.m_mode == GPIO_MODE_INPUT)
        {
            // This will not add the poller if it is already there
            gCamera->m_poller->pollAdd(gpInputPoll);
        } else {
            // This will safely do nothing if there is no poller
            gCamera->m_poller->pollRemove(gpInputPoll);
        }
    }

}

extern "C" void NewGpioModeSelected
    (GtkWidget* widget, GdkEventExpose* event, gpointer userdata )
{
    if (! gCamera || ! gCamera) return;
    if (gGpioTab->m_numRefreshRequestsOutstanding) return;

    // the user has changed the 'mode' of the GPIO, reset all of the 'parameters back
    // to their default state.
    PxLGpioInfo defaultInfo;
    int modeIndex = gGpioTab->m_gpioMode->getSelectedItem();
    defaultInfo.m_mode = (float)gGpioTab->m_supportedGpioModes[modeIndex];
    UpdateGpioInfo (defaultInfo);

    gtk_widget_set_sensitive (gGpioTab->m_gpioUpdate, true);
}

extern "C" void GpioParamChanged
    (GtkWidget* widget, GdkEventExpose* event, gpointer userdata )
{
    if (! gCamera || ! gCamera) return;
    if (gGpioTab->m_numRefreshRequestsOutstanding) return;

    gtk_widget_set_sensitive (gGpioTab->m_gpioUpdate, true);
}

extern "C" void GpioUpdateButtonPressed
    (GtkWidget* widget, GdkEventExpose* event, gpointer userdata )
{
    if (! gCamera || ! gCamera) return;
    if (gGpioTab->m_numRefreshRequestsOutstanding) return;

    // Disable the update button once it has been pressed
    gtk_widget_set_sensitive (gGpioTab->m_gpioUpdate, false);

    GpioEnableToggled  (widget, event, userdata);
}

extern "C" void EventsClearButtonPressed
    (GtkWidget* widget, GdkEventExpose* event, gpointer userdata )
{
    GtkTextBuffer *buf = gtk_text_view_get_buffer (GTK_TEXT_VIEW (gGpioTab->m_events));
    gtk_text_buffer_set_text (buf, "", -1);
}

static U32 PxLEventCallback (
        HANDLE hCamera,
		U32    eventId,
		double eventTimestamp,
		U32    numDataBytes,
        LPVOID pData,
        LPVOID pContext)
{
    if (pContext)
    {
        // We are not doing any camera operations, but it possible to get multiple events in a very short
    	// period of time.  This will serialize them
    	PxLAutoLock lock(&gCameraLock);

        PxLGpio *pControls = (PxLGpio *)pContext;

        // Format a new string for the event
        char newLine[100];
        U32    days = static_cast<U32>(eventTimestamp / (60.0*60.0*24.0));
        eventTimestamp -= days * (60.0*60.0*24.0);
        U32    hours = static_cast<U32>(eventTimestamp / (60.0*60.0));
        eventTimestamp -= hours * (60.0*60.0);
        U32    mins = static_cast<U32>(eventTimestamp / 60.0);
        eventTimestamp -= mins * 60.0;
        snprintf (newLine, sizeof(newLine), ("%03d:%02d:%02d:%04.2f - %s (%d)\r\n"), days, hours, mins, eventTimestamp,
        		  eventId <= EVENT_LAST ? PxLEventNames[eventId] : "Unknown Event", eventId);

        // Append the event
        GtkTextBuffer *buf = gtk_text_view_get_buffer (GTK_TEXT_VIEW (pControls->m_events));
        GtkTextIter   iter;
        gtk_text_buffer_get_iter_at_offset (buf, &iter, -1);

        gtk_text_buffer_insert (buf, &iter, newLine, -1);
    }

    return ApiSuccess;
}



