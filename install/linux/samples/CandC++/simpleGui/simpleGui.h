
/***************************************************************************
 *
 *     File: simpleGui.h
 *
 *     Description: Top level header file for the application.
 *
 *     Notes:  See design notes at at the top of simpleGui.cpp
 *
 */

#if !defined(PIXELINK_SIMPLE_GUI_H)
#define PIXELINK_SIMPLE_GUI_H

#include "camera.h"
#include "cameraSelectCombo.h"
#include "previewButtons.h"

// The currently selected camera.  a NULL value indicates no camera has been selected
extern PxLCamera  *pCamera;

// Our GUI control objects.
extern PxLCameraSelectCombo *csObject;
extern PxLPreviewButtons *previewObject;

#endif // !defined(PIXELINK_SIMPLE_GUI_H)
