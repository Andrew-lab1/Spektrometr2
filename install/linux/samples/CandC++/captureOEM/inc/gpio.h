/***************************************************************************
 *
 *     File: gpio.h
 *
 *     Description:
 *         Controls for the 'GPIO' tab  in CaptureOEM.
 *
 */

#if !defined(PIXELINK_GPIO_H)
#define PIXELINK_GPIO_H

#include <gtk/gtk.h>
#include <stdio.h>
#include <PixeLINKApi.h>
#include <vector>
#include "tab.h"
#include "helpers.h"

class PxLGpio : public PxLTab
{
public:
    typedef enum _HW_TRIGGER_MODES
    {
        MODE_0,
        MODE_1,
        MODE_14
    } HW_TRIGGER_MODES;

    // Pixelink cameras support a wide variety of differnt GPIO configurations.  To help simply the representationused by a specific
    // camera, the GPIO arrangment can be categorized into one of the following 'profiles'
    typedef enum _GpioProfiles
    {
                           //     Product Families                  Description
        NO_GPIOS,          //         PL-A/B/D                No GPIOs at all
        FOUR_GPOS,         //         PL-A/B                  GPIO#1...GPIO#4, all of them GPOs
        ONE_GPO,           //   older auto-focus PL-D         GPIO#1  is a GPO
        ONE_GPIO,          //   newer auto-focus PL-D         GPIO#1.  IS configurable as a GPO or GPI
        ONE_GPIO_ONE_GPO,  //       newer PL-D                GPIO#1 IS configurable as a GPO or GPI.  GPO#2 is a GPO
        TWO_GPOS,          //       older PL-D                GPIO#1 and GPO#2 are GPOs.
        TWO_GPOS_ONE_GPI,  //         PL-X                    GPIO#1 and GPO#2 are GPOs.  GPIO#3 is a GPI
        FOUR_FLEXIBLE_GPIOS, // Added 2024-02 for Creaform    4 GPIOS, configurable as Input or Output.  Also, one can be hardware trigger.
    } GpioProfiles;
    
    // Constructor
    PxLGpio (GtkBuilder *builder);
    // Destructor
    ~PxLGpio ();

    void activate ();   // the user has selected this tab
    void deactivate (); // the user has un-selected this tab
    void refreshRequired (bool noCamera);  // Camera status has changed, requiring a refresh of controls

    //
    // All of the controls

    PxLComboBox  *m_triggerType;

    GtkWidget    *m_swTriggerButton;

    GtkWidget    *m_hwTriggerMode;
    GtkWidget    *m_hwTriggePolarity;
    GtkWidget    *m_hwTriggerDelay;
    GtkWidget    *m_hwTriggerParam1Type;
    GtkWidget    *m_hwTriggerNumber;
    GtkWidget    *m_hwTriggerUpdate;
    GtkWidget    *m_hwTriggerDescription;

    GtkWidget    *m_gpioNumber;
    GtkWidget    *m_gpioEnable;
    PxLComboBox  *m_gpioMode;
    GtkWidget    *m_gpioPolarity;
    GtkWidget    *m_gpioParam1Type;
    GtkWidget    *m_gpioParam1Value;
    GtkWidget    *m_gpioParam1Units;
    GtkWidget    *m_gpioParam2Type;
    GtkWidget    *m_gpioParam2Value;
    GtkWidget    *m_gpioParam2Units;
    GtkWidget    *m_gpioParam3Type;
    GtkWidget    *m_gpioParam3Value;
    GtkWidget    *m_gpioParam3Units;
    GtkWidget    *m_gpioUpdate;
    GtkWidget    *m_gpioDescription;

    GtkWidget	 *m_actionCommandType;
    GtkWidget	 *m_actionCommandDelay;
    GtkWidget    *m_actionSendButton;

    GtkWidget	 *m_events;
    GtkWidget	 *m_eventsClearButton;

    // These 'link' the trigger and GPOs, to Actions
    bool         m_supportsFrameAction;
    bool         m_supportsGpoAction;
    GpioProfiles m_supportedGpos;

    std::vector<int>   m_supportedHwTriggerModes;
    std::vector<int>   m_supportedGpioModes;
    std::vector<int>   m_supportedActions;

    bool m_gpiLast; // last read state of the GP Input

    bool InRange(int value, int min, int max);
    HW_TRIGGER_MODES ModeToIndex(float trigMode);
    float IndexToMode (HW_TRIGGER_MODES);
    GpioProfiles GetGpioProfile(int numGpios, int maxMode);
};

inline bool PxLGpio::InRange(int value, int min, int max)
{
    return (value >= min && value <= max);
}

inline PxLGpio::HW_TRIGGER_MODES PxLGpio::ModeToIndex(float trigMode)
{
    switch ((int)trigMode)
    {
    case 0:  return PxLGpio::MODE_0;
    case 1:  return PxLGpio::MODE_1;
    case 14: return PxLGpio::MODE_14;
    default: return PxLGpio::MODE_0;
    }
}

inline float PxLGpio::IndexToMode(HW_TRIGGER_MODES index)
{
    switch (index)
    {
    case MODE_0:  return 0.0;
    case MODE_1:  return 1.0;
    case MODE_14: return 14.0;
    default:      return 0.0;
    }
}

/**
* Function: GetGpioProfile
* Purpose:  Identifies the GPIO profile of this particular camera
*/
inline PxLGpio::GpioProfiles PxLGpio::GetGpioProfile(int numGpios, int maxMode)
{
    switch (numGpios)
    {
    case 4:
       if (maxMode >= GPIO_MODE_INPUT) return FOUR_FLEXIBLE_GPIOS;
       else                            return FOUR_GPOS;
    case 3:
        return TWO_GPOS_ONE_GPI;
    case 2:
        if (maxMode >= GPIO_MODE_INPUT) return ONE_GPIO_ONE_GPO;
        else                            return TWO_GPOS;
    case 1:
        if (maxMode >= GPIO_MODE_INPUT) return ONE_GPIO;
        else                            return ONE_GPO;
    case 0:
    default:
        return NO_GPIOS;
    }
}



#endif // !defined(PIXELINK_GPIO_H)
