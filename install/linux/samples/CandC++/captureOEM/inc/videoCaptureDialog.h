
/***************************************************************************
 *
 *     File: videoCaptureDialog.h
 *
 *     Description: Modal dialog used while a videw capture is in progress
 *
 */

#if !defined(PIXELINK_VIDEO_CAPTURE_DIALOG_H)
#define PIXELINK_VIDEO_CAPTURE_DIALOG_H

#include <gtk/gtk.h>
#include "PixeLINKApi.h"

class PxLVideoCaptureDialog
{
public:
    // Constructor
	PxLVideoCaptureDialog (GtkBuilder *builder);
	// Destructor
	~PxLVideoCaptureDialog ();

    void begin(int estimatedCaptureTime, bool* stillRunning);  // captureTime is in milliseconds
    bool inProgress();
    void updateDialog(int captureTimeRemaining);

    GtkWidget  *m_windowVideoCapture;

	GtkWidget  *m_progress;
	int         m_captureTime;
    ULONG       m_pollNum;

    bool*       m_stillRunning;

};

inline bool PxLVideoCaptureDialog::inProgress()
{
   return *m_stillRunning;
}

#endif // !defined(PIXELINK_VIDEO_CAPTURE_DIALOG_H)
