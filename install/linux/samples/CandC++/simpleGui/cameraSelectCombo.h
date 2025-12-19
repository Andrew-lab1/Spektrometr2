
/***************************************************************************
 *
 *     File: cameraSelectCombo.h
 *
 *     Description: Simple wrapper class for the camera select combo list
 *
 */

#if !defined(PIXELINK_CAMERA_SELECT_COMBO_H)
#define PIXELINK_CAMERA_SELECT_COMBO_H

#include "PixeLINKApi.h"
#include <vector>
#include <gtk/gtk.h>

class PxLCameraSelectCombo
{
public:
    // Constructor
	PxLCameraSelectCombo (GtkWidget *combo);
	// Destructor
	~PxLCameraSelectCombo ();

    static void  rebuildCameraSelectCombo (ULONG activeCamera);
    ULONG getSelectedCamera();

    PXL_RETURN_CODE scanForCameras ();
    bool            isConnected (ULONG serialNum);

    GtkWidget    *m_csCombo;

    std::vector<ULONG> m_comboCameraList;     // The set of cameras represented in the combo list
    std::vector<ULONG> m_connectedCameraList; // The set of cameras currently connected
    ULONG			   m_selectedCamera;      // The camera currently selected (or 0 for No Camera)
    ULONG			   m_requestedCamera;     // The camera to be selected (or 0 for No Camera)

    bool	m_rebuildInProgress;
    bool    m_scanThreadRunning;

    GThread    		  *m_scanThread;

private:

};

#endif // !defined(PIXELINK_CAMERA_SELECT_COMBO_H)
