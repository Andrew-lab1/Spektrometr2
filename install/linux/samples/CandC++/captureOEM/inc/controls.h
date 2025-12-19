
/***************************************************************************
 *
 *     File: controls.h
 *
 *     Description:
 *         Controls for the 'Controls' tab  in CaptureOEM.
 *
 */

#if !defined(PIXELINK_CONTROLS_H)
#define PIXELINK_CONTROLS_H

#include <vector>
#include <gtk/gtk.h>
#include "slider.h"
#include "tab.h"

class PxLControls : public PxLTab
{
public:
    // Constructor
    PxLControls (GtkBuilder *builder);
	// Destructor
	~PxLControls ();

    void activate ();   // the user has selected this tab
    void deactivate (); // the user has un-selected this tab
    void refreshRequired (bool noCamera);  // Camera status has changed, requiring a refresh of controls
    void streamChange (bool streaming); // Cameras streaming state has changed

    float m_exposureLast; // in millisecond units
    float m_gainLast;
    float m_framerateLast;       // in frames/second
    float m_framerateActualLast; // in frames/second
    float m_redLast;
    float m_greenLast;
    float m_blueLast;

    bool  m_cameraSupportsActualFramerate;  // Cache this value to avoid trashing.

    //
    // All of the controls

    GtkWidget    *m_exposureLabel;
    PxLSlider    *m_exposureSlider;
    GtkWidget    *m_exposureOneTime;
    GtkWidget    *m_exposureContinous;
    GtkWidget    *m_exposureAutoMin;
    GtkWidget    *m_exposureAutoMax;
    GtkWidget    *m_exposureSetAutoMin;
    GtkWidget    *m_exposureSetAutoMax;
    PxLSlider    *m_framerateSlider;
    GtkWidget    *m_framerateFixed;
    GtkWidget    *m_framerateContinous;
    GtkWidget    *m_framerateActual;
    PxLSlider    *m_gammaSlider;
    GtkWidget    *m_gammaEnable;
    PxLSlider    *m_gainSlider;
    GtkWidget    *m_gainOneTime;
    GtkWidget    *m_gainContinous;
    GtkWidget    *m_hdrCombo;
    PxLSlider    *m_colortempSlider;
    GtkWidget    *m_colortempEnable;
    PxLSlider    *m_saturationSlider;
    PxLSlider    *m_brightnessSlider;
    PxLSlider    *m_redSlider;
    PxLSlider    *m_greenSlider;
    PxLSlider    *m_blueSlider;
    GtkWidget    *m_whitebalanceOneTime;

};


#endif // !defined(PIXELINK_CONTROLS_H)
