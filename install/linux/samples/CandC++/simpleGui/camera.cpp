/***************************************************************************
 *
 *     File: camera.cpp
 *
 *     Description: Class definition for a very simple camera.
 */

#include "simpleGui.h"

#include <unistd.h>
#include <gtk/gtk.h>

using namespace std;

// Prototypes
extern "C" void StopButtonPressed
  (GtkWidget* widget, GdkEventExpose* event, gpointer userdata);


static gboolean previewStop (gpointer pData)
{
	// If the user presses the little red X in the preview window, then the API
	// treats this just like the user calling PxLSetPreviewState STOP_PREVIEW.  We
	// want to make this look the same as the user having pressed the stop button.

	StopButtonPressed (NULL, NULL, NULL);

	return false; //  false == only run once
}

extern "C" U32 previewWindowEvent
  (HANDLE hCamera, U32 event, LPVOID pdata)
{
	if (PREVIEW_CLOSED == event)
	{
		gdk_threads_add_idle ((GSourceFunc)previewStop, NULL);
	}

	return 0;
}

PxLCamera::PxLCamera (ULONG serialNum)
: m_serialNum(0)
, m_hCamera(NULL)
, m_streamState(STOP_STREAM)
, m_previewState(STOP_PREVIEW)
{
	PXL_RETURN_CODE rc = ApiSuccess;
	char  title[40];

	rc = PxLInitializeEx (serialNum, &m_hCamera, 0);
	//if (!API_SUCCESS(rc) && rc != ApiNoCameraError)
	if (!API_SUCCESS(rc))
	{
		throw PxLError(rc);
	}

	// Set the preview window to a fixed size.
	sprintf (title, "Preview - Camera %d", m_serialNum);
	rc = PxLSetPreviewSettings (m_hCamera, title, 0, 128, 128, 1024, 768);
    if (!API_SUCCESS(rc))
    {
        throw PxLError(rc);
    }

	m_serialNum = serialNum;

}

PxLCamera::~PxLCamera()
{
	PxLUninitialize (m_hCamera);
}

PXL_RETURN_CODE PxLCamera::play()
{
	PXL_RETURN_CODE rc = ApiSuccess;
	ULONG currentStreamState = m_streamState;

	// Start the camera stream, if necessary
	if (START_STREAM != currentStreamState)
	{
		rc = PxLSetStreamState (m_hCamera, START_STREAM);
		if (!API_SUCCESS(rc)) return rc;
	}

	// now, start the preview
	rc = PxLSetPreviewStateEx(m_hCamera, START_PREVIEW, &m_previewHandle, NULL, previewWindowEvent);
	if (!API_SUCCESS(rc))
	{
		PxLSetStreamState (m_hCamera, currentStreamState);
		m_streamState = STOP_STREAM;
		return rc;
	}
	m_streamState = START_STREAM;
	m_previewState = START_PREVIEW;

	return ApiSuccess;
}

PXL_RETURN_CODE PxLCamera::pause()
{
	PXL_RETURN_CODE rc = ApiSuccess;

	rc = PxLSetPreviewState (m_hCamera, PAUSE_PREVIEW, &m_previewHandle);
	if (!API_SUCCESS(rc)) return rc;

	// We we wanted, we can also pause the stream.  This will make the bus quieter, and
	// a little less load on the system.
	rc = PxLSetStreamState (m_hCamera, PAUSE_STREAM);
	if (!API_SUCCESS(rc)) return rc;
	m_streamState = PAUSE_STREAM;

	m_previewState = PAUSE_PREVIEW;

	return ApiSuccess;
}

PXL_RETURN_CODE PxLCamera::stop()
{
	PXL_RETURN_CODE rc = ApiSuccess;

	rc = PxLSetPreviewState(m_hCamera, STOP_PREVIEW, &m_previewHandle);
	if (!API_SUCCESS(rc)) return rc;
	m_previewState = STOP_PREVIEW;

	rc = PxLSetStreamState (m_hCamera, STOP_STREAM);
	m_streamState = STOP_STREAM;

	return rc;
}




