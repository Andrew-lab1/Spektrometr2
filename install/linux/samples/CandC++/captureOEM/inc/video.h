/***************************************************************************
 *
 *     File: video.h
 *
 *     Description:
 *         Controls for the 'Video' tab  in CaptureOEM.
 *
 */

#if !defined(PIXELINK_VIDEO_H)
#define PIXELINK_VIDEO_H

#include <gtk/gtk.h>
#include <stdio.h>
#include <PixeLINKApi.h>
#include "slider.h"
#include "tab.h"

class PxLVideo : public PxLTab
{
public:
    typedef enum _ENCODING_TYPES
    {
        H_264,
        H_265,
    } ENCODING_TYPES;

    // Constructor
    PxLVideo (GtkBuilder *builder);
    // Destructor
    ~PxLVideo ();

    ENCODING_TYPES EncodingToIndex(U32 encoding);
    U32 IndextoEncoding(ENCODING_TYPES index);

    void activate ();   // the user has selected this tab
    void deactivate (); // the user has un-selected this tab
    void refreshRequired (bool noCamera);  // Camera status has changed, requiring a refresh of controls

    //
    // All of the controls

    GtkWidget    *m_fileName;
    GtkWidget    *m_fileType;
    GtkWidget    *m_fileLocation;
    GtkWidget    *m_fileLocationBrowser;

    GtkWidget    *m_fileNameIncrement;
    GtkWidget    *m_captureLaunch;

    GtkWidget    *m_encodingType;
    GtkWidget    *m_numFramesToCapture;
    GtkWidget    *m_decimation;
    GtkWidget    *m_keepIntermidiate;

    GtkWidget    *m_fpsCamera;
    GtkWidget    *m_fpsPlayback;
    GtkWidget    *m_fpsMatch;
    GtkWidget    *m_fpsComment;

    GtkWidget    *m_bitrateAuto;
    PxLSlider    *m_bitrateSlider;

    GtkWidget    *m_recordTime;
    GtkWidget    *m_playbackTime;
    GtkWidget    *m_fileSize;

    GtkWidget    *m_captureButton;

    bool  m_captureInProgress;

    // Once we start a capture, remember the file names so that we don't have to do string manipulations multiple times.
    gchar* m_encodedFilename;
    gchar* m_videoFilename;

    // If the decimation factor changes, we need to compute a new playback rate and playback time.  However,
    // we cannot compute both of these with just a new decimation value (and number of frames) -- we need to
    // also know how much decimation was applied to the old playback rate and time.  In other words,
    // we need to know by how MUCH the decimation is changing.  So, keep track of this current decimation
    int m_currentDecimation;
};

inline PxLVideo::ENCODING_TYPES PxLVideo::EncodingToIndex(U32 encoding)
{
    switch (encoding)
    {
    default:
    case CLIP_ENCODING_H264:  return PxLVideo::H_264;
    case CLIP_ENCODING_H265:  return PxLVideo::H_265;
    }
}

inline U32 PxLVideo::IndextoEncoding(PxLVideo::ENCODING_TYPES index)
{
    switch (index)
    {
    default:
    case PxLVideo::H_264:      return CLIP_ENCODING_H264;
    case PxLVideo::H_265:      return CLIP_ENCODING_H265;
    }
}


#endif // !defined(PIXELINK_VIDEO_H)
