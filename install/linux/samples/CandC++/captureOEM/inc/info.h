/***************************************************************************
 *
 *     File: info.h
 *
 *     Description:
 *         Controls for the 'info' tab  in CaptureOEM.
 *
 */

#if !defined(PIXELINK_INFO_H)
#define PIXELINK_INFO_H

#include <gtk/gtk.h>
#include <stdio.h>
#include <PixeLINKApi.h>
#include "camera.h"
#include "captureOEM.h"
#include "tab.h"


class PxLInfo : public PxLTab
{
public:

    // Constructor
    PxLInfo (GtkBuilder *builder);
    // Destructor
    ~PxLInfo ();

    void activate ();   // the user has selected this tab
    void deactivate (); // the user has un-selected this tab
    void refreshRequired (bool noCamera);  // Camera status has changed, requiring a refresh of controls

    //
    // All of the controls

    GtkWidget    *m_loadSettingsButton;
    GtkWidget    *m_factoryDefaults;
    GtkWidget    *m_powerupDefaults;
    GtkWidget    *m_configurationFile;
    GtkWidget    *m_configurationFileName;
    GtkWidget    *m_configurationFileLocation;
    GtkWidget    *m_configurationFileLocationBrowser;
    GtkWidget    *m_saveSettingsButton;

    GtkWidget    *m_tempSensor;
    GtkWidget    *m_tempBody;

    GtkWidget    *m_serialNum;
    GtkWidget    *m_vendorName;
    GtkWidget    *m_productName;
    GtkWidget    *m_cameraName;
    GtkWidget    *m_cameraIPAddress;

    GtkWidget    *m_versionFirmware;
    GtkWidget    *m_versionFpga;
    GtkWidget    *m_versionXml;

    GtkWidget    *m_versionPackage;
    GtkWidget    *m_versionCaptureOem;

    GtkWidget    *m_libraryInfo;

    float m_sensorTempLast;  // the last read sensor temperature
    float m_bodyTempLast;    // the last read camera body temperature

    // Not all camera have a body and sensor temperature sensor
    bool m_hasSensorTemperature;
    bool m_hasBodyTemperature;
};

#endif // !defined(PIXELINK_INFO_H)
