/***************************************************************************
 *
 *     File: filter.h
 *
 *     Description:
 *         Controls for the 'filter' tab  in CaptureOEM.
 *
 */

#if !defined(PIXELINK_FILTER_H)
#define PIXELINK_FILTER_H

#include <gtk/gtk.h>
#include <stdio.h>
#include <PixeLINKApi.h>
#include <SDL2/SDL.h>
#include "camera.h"
#include "tab.h"

#define PXLAPI_CALLBACK(funcname)                         \
    U32 funcname( HANDLE hCamera,               \
                  LPVOID pFrameData,            \
                  U32 uDataFormat,              \
                  FRAME_DESC const * pFrameDesc,\
                  LPVOID pContext)              \

class PxLFilter : public PxLTab
{
public:
    typedef enum _PREVIEW_FILTERS
    {
        FILTER_NONE,
        FILTER_NEGATIVE,
        FILTER_GRAYSCALE,
        FILTER_HISTOGRAM_EQUALIZATION,
        FILTER_SATURATED_AND_BLACK,
        FILTER_THRESHOLD_50_PERCENT,
        FILTER_LOW_PASS,
        FILTER_MEDIAN,
        FILTER_HIGH_PASS,
        FILTER_SOBEL,
        FILTER_TEMPORAL_THRESHOLD,
        FILTER_MOTION_DETECTOR,
        FILTER_ASCII,
        FILTER_BITMAP_OVERLAY,
        FILTER_CROSSHAIR_OVERLAY,
        NUM_PREVIEW_FILTERS
    } PREVIEW_FILTERS;

    // Constructor
    PxLFilter (GtkBuilder *builder);
    // Destructor
    ~PxLFilter ();

    void activate ();   // the user has selected this tab
    void deactivate (); // the user has un-selected this tab
    void refreshRequired (bool noCamera);  // Camera status has changed, requiring a refresh of controls

    //
    // All of the controls

    GtkWidget    *m_previewFilter;
    GtkWidget    *m_filterWarning;
    GtkWidget    *m_filterLocation;
    GtkWidget    *m_filterLocationBrowser;


    // The bitmap overlay to be used by the bitmap overlay callback.  NULL if not valid.
    SDL_Surface* m_bitmapOverlay;
};

#endif // !defined(PIXELINK_FILTER_H)
