
/***************************************************************************
 *
 *     File: cameraSelect.h
 *
 *     Description:
 *         Controls for the 'Camera Select' and 'Video Preview' controls
 *         in CaptureOEM.
 *
 */

#if !defined(PIXELINK_PREVIEW_H)
#define PIXELINK_PREVIEW_H

#include <gtk/gtk.h>

class PxLPreview
{
public:
    // Constructor
	PxLPreview (GtkBuilder *builder);
	// Destructor
	~PxLPreview ();

    void refreshRequired (bool noCamera);  // Used to indicate when we have a new camera, or no camera

    GtkWidget    *m_play;
    GtkWidget    *m_pause;
    GtkWidget    *m_stop;

    GtkWidget    *m_autoResize;

};

#endif // !defined(PIXELINK_PREVIEW_H)
