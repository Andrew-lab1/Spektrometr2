/***************************************************************************
 *
 *     File: filter.cpp
 *
 *     Description:
 *        Controls for the 'Filter' tab  in CaptureOEM.
 */

#include <string>
#include "filter.h"
#include "camera.h"
#include "captureOEM.h"
#include "helpers.h"
#include "cameraSelect.h"

using namespace std;

extern PxLFilter      *gFilterTab;


//
// Local prototypes.
//    UI updates can only be done from a gtk thread -- these routines are gtk 'idle' threads
//    and as such are 'UI update safe'. For each 'feature', there there the following functions:
//       . {featureXXX}Deactivate - Makes the controls meaningless (including greying them out)
//       . {featreuXXX}Activate - updates the controls with values from the camera
static gboolean  RefreshComplete (gpointer pData);
static gboolean  FilterDeactivate (gpointer pData);
static gboolean  FilterActivate (gpointer pData);

extern PXLAPI_CALLBACK (PxLCallbackNegative);
extern PXLAPI_CALLBACK (PxLCallbackGrayscale);
extern PXLAPI_CALLBACK (PxLCallbackHistogramEqualization);
extern PXLAPI_CALLBACK (PxLCallbackSaturatedAndBlack);
extern PXLAPI_CALLBACK (PxLCallbackTreshold50Percent);
extern PXLAPI_CALLBACK (PxLCallbackLowPass);
extern PXLAPI_CALLBACK (PxLCallbackMedian);
extern PXLAPI_CALLBACK (PxLCallbackHighPass);
extern PXLAPI_CALLBACK (PxLCallbackSobel);
extern PXLAPI_CALLBACK (PxLCallbackTemporalTheshold);
extern PXLAPI_CALLBACK (PxLCallbackMotionDetector);
extern PXLAPI_CALLBACK (PxLCallbackAscii);
extern PXLAPI_CALLBACK (PxLCallbackBitmapOverlay);
extern PXLAPI_CALLBACK (PxLCallbackCrosshairOverlay);

// Indexed by PxLFilter::PREVIEW_FILTERS
static PxLApiCallback Callbacks[] =
{
   NULL,
   PxLCallbackNegative,
   PxLCallbackGrayscale,
   PxLCallbackHistogramEqualization,
   PxLCallbackSaturatedAndBlack,
   PxLCallbackTreshold50Percent,
   PxLCallbackLowPass,
   PxLCallbackMedian,
   PxLCallbackHighPass,
   PxLCallbackSobel,
   PxLCallbackTemporalTheshold,
   PxLCallbackMotionDetector,
   PxLCallbackAscii,
   PxLCallbackBitmapOverlay,
   PxLCallbackCrosshairOverlay
};

/* ---------------------------------------------------------------------------
 * --   Member functions - Public
 * ---------------------------------------------------------------------------
 */
PxLFilter::PxLFilter (GtkBuilder *builder)
: m_bitmapOverlay (NULL)
{
    //
    // Step 1
    //      Find all of the glade controls

    m_previewFilter = GTK_WIDGET( gtk_builder_get_object( builder, "PreviewFilter_Combo" ) );
    m_filterWarning = GTK_WIDGET( gtk_builder_get_object( builder, "FilterDesc_Label" ) );
    m_filterLocation = GTK_WIDGET( gtk_builder_get_object( builder, "FilterLocation_Text" ) );
    m_filterLocationBrowser = GTK_WIDGET( gtk_builder_get_object( builder, "FilterLocation_Button" ) );

    //
    // Step 2
    //      Initialize the dropdown with all of the fiters
    gtk_combo_box_text_insert_text (GTK_COMBO_BOX_TEXT(m_previewFilter), FILTER_NONE, "None");
    gtk_combo_box_text_insert_text (GTK_COMBO_BOX_TEXT(m_previewFilter), FILTER_NEGATIVE, "Negative");
    gtk_combo_box_text_insert_text (GTK_COMBO_BOX_TEXT(m_previewFilter), FILTER_GRAYSCALE, "Grayscale");
    gtk_combo_box_text_insert_text (GTK_COMBO_BOX_TEXT(m_previewFilter), FILTER_HISTOGRAM_EQUALIZATION, "Histogram Equalization");
    gtk_combo_box_text_insert_text (GTK_COMBO_BOX_TEXT(m_previewFilter), FILTER_SATURATED_AND_BLACK, "Saturated and Black");
    gtk_combo_box_text_insert_text (GTK_COMBO_BOX_TEXT(m_previewFilter), FILTER_THRESHOLD_50_PERCENT, "Threshold - 50%");
    gtk_combo_box_text_insert_text (GTK_COMBO_BOX_TEXT(m_previewFilter), FILTER_LOW_PASS, "Low Pass");
    gtk_combo_box_text_insert_text (GTK_COMBO_BOX_TEXT(m_previewFilter), FILTER_MEDIAN, "Median");
    gtk_combo_box_text_insert_text (GTK_COMBO_BOX_TEXT(m_previewFilter), FILTER_HIGH_PASS, "High Pass");
    gtk_combo_box_text_insert_text (GTK_COMBO_BOX_TEXT(m_previewFilter), FILTER_SOBEL, "Sobel");
    gtk_combo_box_text_insert_text (GTK_COMBO_BOX_TEXT(m_previewFilter), FILTER_TEMPORAL_THRESHOLD, "Temporal Threshold");
    gtk_combo_box_text_insert_text (GTK_COMBO_BOX_TEXT(m_previewFilter), FILTER_MOTION_DETECTOR, "Motion Detector");
    gtk_combo_box_text_insert_text (GTK_COMBO_BOX_TEXT(m_previewFilter), FILTER_ASCII, "ASCII");
    gtk_combo_box_text_insert_text (GTK_COMBO_BOX_TEXT(m_previewFilter), FILTER_BITMAP_OVERLAY, "Bitmap Overlay");
    gtk_combo_box_text_insert_text (GTK_COMBO_BOX_TEXT(m_previewFilter), FILTER_CROSSHAIR_OVERLAY, "Crosshair Overlay");

    gtk_combo_box_set_active (GTK_COMBO_BOX(m_previewFilter), FILTER_NONE);

    //
    // Step 3
    //  Initialize our bitmap overlay defaults
    gchar* bitmapFolder = g_strdup_printf ("%s/Pictures", g_get_home_dir());
    gtk_entry_set_text (GTK_ENTRY(m_filterLocation), bitmapFolder);
    gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER(m_filterLocationBrowser),
                                         bitmapFolder);
    g_free(bitmapFolder);
}


PxLFilter::~PxLFilter ()
{
}

void PxLFilter::refreshRequired (bool noCamera)
{
    if (IsActiveTab (FilterTab))
    {
        if (noCamera)
        {
            // If I am the active tab, then grey out everything
            gdk_threads_add_idle ((GSourceFunc)FilterDeactivate, this);
        } else {
            // If I am the active tab, then refresh everything
            gdk_threads_add_idle ((GSourceFunc)FilterActivate, this);
        }

        gdk_threads_add_idle ((GSourceFunc)RefreshComplete, this);
        m_numRefreshRequestsOutstanding++;
    } else {
        // If we are not the active tab, only bump the m_numRefreshRequestsOutstanding if there is not
        // one outstanding already; RefreshComplete will be scheduled when the tab becomes active
        if (!m_numRefreshRequestsOutstanding)m_numRefreshRequestsOutstanding++;
    }
}

void PxLFilter::activate()
{
    // I have become the active tab.

    if (gCamera)
    {
        if (m_numRefreshRequestsOutstanding)
        {
            gdk_threads_add_idle ((GSourceFunc)FilterActivate, this);
        }
    } else {
        gdk_threads_add_idle ((GSourceFunc)FilterDeactivate, this);
    }
    m_numRefreshRequestsOutstanding = 1; // As a safety mechanism, tab activation should assert value, it will be set to 0 when RefreshComplete
    gdk_threads_add_idle ((GSourceFunc)RefreshComplete, this);
}

void PxLFilter::deactivate()
{
    // I am no longer the active tab.
}


/* ---------------------------------------------------------------------------
 * --   gtk thread callbacks - used to update controls
 * ---------------------------------------------------------------------------
 */

// Indicate that the refresh is no longer outstanding, it has completed.
static gboolean RefreshComplete (gpointer pData)
{
    PxLFilter *pFilter = (PxLFilter *)pData;

    pFilter->m_numRefreshRequestsOutstanding--;
    return false;
}

//
// Make Filter controls unselectable
static gboolean FilterDeactivate (gpointer pData)
{
    PxLFilter *pFilter = (PxLFilter *)pData;

    // The only control that is ever deactivated, is the capture button

    gtk_widget_set_sensitive (pFilter->m_previewFilter, false);
    gtk_widget_set_sensitive (pFilter->m_filterLocationBrowser, false);

    // The camera is 'gone', and any callback has been canceled.  Represent this
    // in the dropdown.
    gtk_combo_box_set_active (GTK_COMBO_BOX(pFilter->m_previewFilter), PxLFilter::FILTER_NONE);

    // No longer going a bitmap overlay, so free the bitmap surface (if there is one)
    if (pFilter->m_bitmapOverlay)
    {
        SDL_FreeSurface (pFilter->m_bitmapOverlay);
        pFilter->m_bitmapOverlay = NULL;
    }

    gtk_label_set_text (GTK_LABEL (pFilter->m_filterWarning), " ");

    return false;  //  Only run once....
}

//
// Make filter controls selectable (if appropriate)
static gboolean FilterActivate (gpointer pData)
{
    PxLFilter *pFilter = (PxLFilter *)pData;
    float format = (float)PIXEL_FORMAT_YUV422;

    if (gCamera)
    {
        gtk_widget_set_sensitive (pFilter->m_previewFilter, true);

        // Only activate the bitmap overlay controls if the user has selected the bitmap overlay
        int callbackSelected = gtk_combo_box_get_active (GTK_COMBO_BOX(pFilter->m_previewFilter));
        if (callbackSelected == PxLFilter::FILTER_BITMAP_OVERLAY)
        {
            gtk_widget_set_sensitive (pFilter->m_filterLocationBrowser, true);
        }

        gCamera->getValue(FEATURE_PIXEL_FORMAT, &format);
    }

    if (format == PIXEL_FORMAT_YUV422)
    {
        gtk_widget_set_sensitive (pFilter->m_previewFilter, false);
        gtk_label_set_text (GTK_LABEL (pFilter->m_filterWarning), " Filters not permitted with YUV pixel formats");
    } else {
        gtk_label_set_text (GTK_LABEL (pFilter->m_filterWarning), " ");
    }

    return false;  //  Only run once....
}

/* ---------------------------------------------------------------------------
 * --   Control functions from the Glade project
 * ---------------------------------------------------------------------------
 */

extern "C" void NewPreviewFilterSelected
    (GtkWidget* widget, GdkEventExpose* event, gpointer userdata )
{
    if (! gCamera || ! gFilterTab) return;
    if (gFilterTab->m_numRefreshRequestsOutstanding) return;

    int callbackSelected = gtk_combo_box_get_active (GTK_COMBO_BOX(gFilterTab->m_previewFilter));
    bool bitmapOverlay = false;

    if (gCamera && callbackSelected >= 0 && callbackSelected < PxLFilter::NUM_PREVIEW_FILTERS)
    {
        // If the user selected bitmap overlay, the we need to load the bitmap for the user
        if (callbackSelected == PxLFilter::FILTER_BITMAP_OVERLAY)
        {
            bitmapOverlay = true;
            // unload the old bitmap if we already have one.
            if (gFilterTab->m_bitmapOverlay) SDL_FreeSurface (gFilterTab->m_bitmapOverlay);

            // figure out the complete filename for the bmp
            gFilterTab->m_bitmapOverlay =
                    SDL_LoadBMP (gtk_file_chooser_get_filename (GTK_FILE_CHOOSER(gFilterTab->m_filterLocationBrowser)));
            gtk_entry_set_text (GTK_ENTRY(gFilterTab->m_filterLocation),
                                gtk_file_chooser_get_current_folder (GTK_FILE_CHOOSER(gFilterTab->m_filterLocationBrowser)));
        } else {
            // No longer using a bitmap overlay, so free the bitmap surface (if there is one)
            if (gFilterTab->m_bitmapOverlay)
            {
                SDL_FreeSurface (gFilterTab->m_bitmapOverlay);
                gFilterTab->m_bitmapOverlay = NULL;
            }
        }

        gtk_widget_set_sensitive (gFilterTab->m_filterLocationBrowser, bitmapOverlay);

        // And finally, set the callback (which may actually cancel the callback if NULL were specified).
        gCamera->setPreviewCallback (Callbacks[callbackSelected], gFilterTab->m_bitmapOverlay);
    }
}

extern "C" void NewPreviewLocation
    (GtkWidget* widget, GdkEventExpose* event, gpointer userdata )
{
    // We need to set a new callback, as the bitmap has changed.  This is the same as if the
    // user selected bitmap overaly as the new filter.
    NewPreviewFilterSelected (widget, event, userdata);
}
