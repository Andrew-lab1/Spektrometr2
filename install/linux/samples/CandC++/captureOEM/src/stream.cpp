
/***************************************************************************
 *
 *     File: stream.cpp
 *
 *     Description:
 *        Controls for the 'Stream' tab  in CaptureOEM.
 */

#include <algorithm>
#include "stream.h"
#include "cameraSelect.h"
#include "camera.h"
#include "captureOEM.h"
#include "controls.h"
#include "preview.h"
#if defined VIDEO_CAPTURE_SUPPORTED
#include "video.h"
#endif
#include "lens.h"
#include "filter.h"
#include "autoRoi.h"

using namespace std;


extern PxLStream       *gStreamTab;
extern PxLCameraSelect *gCameraSelectTab;
extern PxLControls     *gControlsTab;
extern PxLPreview      *gVideoPreviewTab;
#if defined VIDEO_CAPTURE_SUPPORTED
extern PxLVideo        *gVideoTab;
#endif
extern PxLLens         *gLensTab;
extern PxLFilter       *gFilterTab;
extern PxLAutoRoi      *gAutoRoiTab;

//
// Local prototypes.
//    UI updates can only be done from a gtk thread -- these routines are gtk 'idle' threads
//    and as such are 'UI update safe'. For each 'feature', there there the following functions:
//       . {featureXXX}Deactivate - Makes the controls meaningless (including greying them out)
//       . {featreuXXX}Activate - updates the controls with values from the camera
static gboolean  RefreshComplete (gpointer pData);
static gboolean  RoiDeactivate (gpointer pData);
static gboolean  RoiActivate (gpointer pData);
static gboolean  PixelFormatDeactivate (gpointer pData);
static gboolean  PixelFormatActivate (gpointer pData);
static gboolean  CompressionDeactivate (gpointer pData);
static gboolean  CompressionActivate (gpointer pData);
static gboolean  PixelFormatInterpretationDeactivate (gpointer pData);
static gboolean  PixelFormatInterpretationActivate (gpointer pData);
static gboolean  PixelAddressingDeactivate (gpointer pData);
static gboolean  PixelAddressingActivate (gpointer pData);
static gboolean  AlphaDeactivate (gpointer pData);
static gboolean  AlphaActivate (gpointer pData);
static gboolean  TransformationsDeactivate (gpointer pData);
static gboolean  TransformationsActivate (gpointer pData);
extern "C" void NewRoiSelected (GtkWidget* widget, GdkEventExpose* event, gpointer userdata);

extern "C" bool RoiButtonPress  (GtkWidget* widget, GdkEventButton *event );
extern "C" bool RoiButtonMotion (GtkWidget* widget, GdkEventButton *event );
extern "C" bool RoiButtonEnter  (GtkWidget* widget, GdkEventButton *event );

/* ---------------------------------------------------------------------------
 * --   Static data used to construct our dropdowns - Private
 * ---------------------------------------------------------------------------
 */

// Indexed by COEM_PIXEL_FORMATS
static const char * const PxLFormatStrings[] =
{
   "Mono8",
   "Mono16",
   "Mono10 Packed",
   "Mono12 Packed",
   "Bayer8",
   "Bayer16",
   "Bayer10 Packed",
   "Bayer12 Packed",
   "YUV422",
   "Stokes",
   "Polar",
   "PolarRaw",
   "HSV",
   "RGB24",
   "BGR24",
   "RGBA",
   "BGRA",
   "ARGB",
   "ABGR",
};

// Indexed by COEM_PIXEL_FORMAT_INTERPRETATIONS
static const char * const PxLFormatInterpretationStrings[] =
{
   "Color",
   "Angle",
   "Degree",
};

// Indexed by COEM_PIXEL_ADDRESS_MODES
static const char * const PxLAddressModes[] =
{
   "Decimate",
   "Average",
   "Binning",
   "Resample"
};

// Indexed by COEM_PIXEL_ADDRESS_VALUES
static const char * const PxLAddressValues[] =
{
   "None",   "1 by 2", "1 by 3", "1 by 4", "1 by 6", "1 by 8",
   "2 by 1", "2 by 2", "2 by 3", "2 by 4", "2 by 6", "2 by 8",
   "3 by 1", "3 by 2", "3 by 3", "3 by 4", "3 by 6", "3 by 8",
   "4 by 1", "4 by 2", "4 by 3", "4 by 4", "4 by 6", "4 by 8",
   "6 by 1", "6 by 2", "6 by 3", "6 by 4", "6 by 6", "6 by 8",
   "8 by 1", "8 by 2", "8 by 3", "8 by 4", "8 by 6", "8 by 8"
};

static const char  RoiLabel[] = "Current\n    ROI";


/* ---------------------------------------------------------------------------
 * --   Member functions - Public
 * ---------------------------------------------------------------------------
 */
PxLStream::PxLStream (GtkBuilder *builder)
: m_ffovBuf(NULL)
, m_currentOp(OP_NONE)
, m_pixelAddressX(1)
, m_pixelAddressY(1)
, m_rotateValue (0)
, m_verticalFlip(false)
, m_horizontalFlip(false)
{
    //
    // Step 1
    //      Find all of the glade controls

    // Roi
    m_roiCombo = GTK_WIDGET( gtk_builder_get_object( builder, "Roi_Combo" ) );
    m_offsetX = GTK_WIDGET( gtk_builder_get_object( builder, "OffsetX_Text" ) );
    m_offsetY = GTK_WIDGET( gtk_builder_get_object( builder, "OffsetY_Text" ) );
    m_width = GTK_WIDGET( gtk_builder_get_object( builder, "Width_Text" ) );
    m_height = GTK_WIDGET( gtk_builder_get_object( builder, "Height_Text" ) );
    m_center = GTK_WIDGET( gtk_builder_get_object( builder, "Center_Button" ) );
    m_ffovImage = GTK_WIDGET( gtk_builder_get_object( builder, "Ffov_Image" ) );
    m_roiButton = GTK_WIDGET( gtk_builder_get_object( builder, "Roi_Button" ) );
    // connect elements from the style sheet, to our builder
    gtk_widget_set_name (m_roiButton, "Roi_Button_red");

    // Pixel Format
    m_pixelFormat = GTK_WIDGET( gtk_builder_get_object( builder, "PixelFormat_Combo" ) );
    m_pixelFormatInterpretation = GTK_WIDGET( gtk_builder_get_object( builder, "PixelFormatInterpretation_Combo" ) );
    m_alpha = GTK_WIDGET( gtk_builder_get_object( builder, "Alpha_Text" ) );

    // Compression
    m_compression = GTK_WIDGET( gtk_builder_get_object( builder, "Compression_Checkbutton" ) );

    // Pixel Addressing
    m_pixelAddressingMode = GTK_WIDGET( gtk_builder_get_object( builder, "PixelAddressingMode_Combo" ) );
    m_pixelAddressingValue = GTK_WIDGET( gtk_builder_get_object( builder, "PixelAddressingValue_Combo" ) );

    // Transformations
    m_rotate = GTK_WIDGET( gtk_builder_get_object( builder, "Rotate_Combo" ) );
    m_flip = GTK_WIDGET( gtk_builder_get_object( builder, "Flip_Combo" ) );

    //
    // Step 3.
    //      Create the cursors we will use for ROI button operations

    // We don't have a mouse cursor yet, we need to wait until the windows are realized.
    //m_originalCursor = gdk_window_get_cursor (gtk_widget_get_window (m_ffovImage));

    m_moveCursor = gdk_cursor_new_for_display (gdk_display_get_default(), GDK_FLEUR);
    m_tlCursor = gdk_cursor_new_for_display (gdk_display_get_default(), GDK_TOP_LEFT_CORNER);
    m_trCursor = gdk_cursor_new_for_display (gdk_display_get_default(), GDK_TOP_RIGHT_CORNER);
    m_brCursor = gdk_cursor_new_for_display (gdk_display_get_default(), GDK_BOTTOM_RIGHT_CORNER);
    m_blCursor = gdk_cursor_new_for_display (gdk_display_get_default(), GDK_BOTTOM_LEFT_CORNER);
    m_tCursor = gdk_cursor_new_for_display (gdk_display_get_default(), GDK_TOP_SIDE);
    m_rCursor = gdk_cursor_new_for_display (gdk_display_get_default(), GDK_RIGHT_SIDE);
    m_bCursor = gdk_cursor_new_for_display (gdk_display_get_default(), GDK_BOTTOM_SIDE);
    m_lCursor = gdk_cursor_new_for_display (gdk_display_get_default(), GDK_LEFT_SIDE);

    // Step 4.
    //      enable the mouse events for the ROI button
    g_signal_connect (m_roiButton, "button_press_event",   G_CALLBACK (RoiButtonPress), NULL);
    g_signal_connect (m_roiButton, "button_release_event", G_CALLBACK (RoiButtonPress), NULL);
    g_signal_connect (m_roiButton, "motion_notify_event",  G_CALLBACK (RoiButtonMotion), NULL);
    g_signal_connect (m_roiButton, "enter_notify_event",   G_CALLBACK (RoiButtonEnter), NULL);
    g_signal_connect (m_roiButton, "leave_notify_event",   G_CALLBACK (RoiButtonEnter), NULL);

    gtk_widget_set_events (m_roiButton,
                           GDK_ENTER_NOTIFY_MASK | GDK_LEAVE_NOTIFY_MASK |
                           GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK |
                           GDK_POINTER_MOTION_MASK | GDK_POINTER_MOTION_HINT_MASK);
}

PxLStream::~PxLStream ()
{
}

void PxLStream::refreshRequired (bool noCamera)
{
    m_ffovBuf = NULL;

    if (IsActiveTab (StreamTab))
    {
        if (noCamera)
        {
            // If I am the active tab, then grey out everything
            gdk_threads_add_idle ((GSourceFunc)RoiDeactivate, this);
            gdk_threads_add_idle ((GSourceFunc)PixelFormatDeactivate, this);
            gdk_threads_add_idle ((GSourceFunc)CompressionDeactivate, this);
            gdk_threads_add_idle ((GSourceFunc)PixelFormatInterpretationDeactivate, this);
            gdk_threads_add_idle ((GSourceFunc)AlphaDeactivate, this);
            gdk_threads_add_idle ((GSourceFunc)PixelAddressingDeactivate, this);
            gdk_threads_add_idle ((GSourceFunc)TransformationsDeactivate, this);
        } else {
            // If I am the active tab, then refresh everything
            gdk_threads_add_idle ((GSourceFunc)RoiActivate, this);
            gdk_threads_add_idle ((GSourceFunc)PixelFormatActivate, this);
            gdk_threads_add_idle ((GSourceFunc)CompressionActivate, this);
            gdk_threads_add_idle ((GSourceFunc)PixelFormatInterpretationActivate, this);
            gdk_threads_add_idle ((GSourceFunc)AlphaActivate, this);
            gdk_threads_add_idle ((GSourceFunc)PixelAddressingActivate, this);
            gdk_threads_add_idle ((GSourceFunc)TransformationsActivate, this);
        }
        gdk_threads_add_idle ((GSourceFunc)RefreshComplete, this);
        m_numRefreshRequestsOutstanding++;
    } else {
        // If we are not the active tab, only bump the m_numRefreshRequestsOutstanding if there is not
        // one outstanding already; RefreshComplete will be scheduled when the tab becomes active
        if (!m_numRefreshRequestsOutstanding)m_numRefreshRequestsOutstanding++;
    }
}

void PxLStream::activate()
{
    // I have become the active tab.

    if (gCamera)
    {
        if (m_numRefreshRequestsOutstanding)
        {
            gdk_threads_add_idle ((GSourceFunc)PixelFormatActivate, this);
            gdk_threads_add_idle ((GSourceFunc)CompressionActivate, this);
            gdk_threads_add_idle ((GSourceFunc)PixelFormatInterpretationActivate, this);
            gdk_threads_add_idle ((GSourceFunc)PixelAddressingActivate, this);
            gdk_threads_add_idle ((GSourceFunc)TransformationsActivate, this);
            gdk_threads_add_idle ((GSourceFunc)RoiActivate, this);
        }
    } else {
        gdk_threads_add_idle ((GSourceFunc)PixelFormatDeactivate, this);
        gdk_threads_add_idle ((GSourceFunc)CompressionDeactivate, this);
        gdk_threads_add_idle ((GSourceFunc)PixelFormatInterpretationDeactivate, this);
        gdk_threads_add_idle ((GSourceFunc)PixelAddressingDeactivate, this);
        gdk_threads_add_idle ((GSourceFunc)TransformationsDeactivate, this);
        gdk_threads_add_idle ((GSourceFunc)RoiDeactivate, this);
    }

    m_numRefreshRequestsOutstanding = 1; // As a safety mechanism, tab activation should assert value, it will be set to 0 when RefreshComplete
    gdk_threads_add_idle ((GSourceFunc)RefreshComplete, this);
}

void PxLStream::deactivate()
{
    // I am no longer the active tab.
}

// indication that the app has transitioned to/from playing state.
void PxLStream::playChange (bool playing)
{
    if (IsActiveTab (StreamTab)) loadFfovImage();
}

// Load the ROI background image.  streamInterrupted is true if the stream is supposed to be running, but
// has temporarily been turned off to perform some operations.
void PxLStream::loadFfovImage(bool streamInterrupted)
{
    //
    // Step 1
    //      If we don't have a camera, load the 'No Camera' bmp
    if (! gCamera)
    {
        // No camera
        if (m_ffovBuf == NULL)
        {
            // Nothing loaded in the FFOV area, load a default message
            m_ffovBuf = gdk_pixbuf_new_from_file_at_size ("NoCamera.bmp",
                                                          FFOV_AREA_WIDTH, FFOV_AREA_HEIGHT, NULL);
            gtk_image_set_from_pixbuf (GTK_IMAGE (m_ffovImage), m_ffovBuf);
        }
    } else {
        PXL_RETURN_CODE rc;

        // Step 2.
        //      Figure out the aspect ration of the imager and FFOV image area
        float ffovImageAspectRatio = (float)FFOV_AREA_WIDTH / (float)FFOV_AREA_HEIGHT;

        if (m_cameraAspectRatio > ffovImageAspectRatio)
        {
            // height limited
            m_roiButtonLimits.xMax = FFOV_AREA_WIDTH;
            m_roiButtonLimits.yMax = (int)((float)FFOV_AREA_WIDTH / m_cameraAspectRatio);
            m_roiButtonLimits.xMin = 0;
            m_roiButtonLimits.yMin = (FFOV_AREA_HEIGHT - m_roiButtonLimits.yMax) / 2 ;
            m_roiButtonLimits.yMax += m_roiButtonLimits.yMin;
        } else {
            // width limited (or unlimited (same aspect ratio)
            m_roiButtonLimits.xMax = (int)((float)FFOV_AREA_HEIGHT * m_cameraAspectRatio);
            m_roiButtonLimits.yMax = FFOV_AREA_HEIGHT;
            m_roiButtonLimits.xMin = (FFOV_AREA_WIDTH - m_roiButtonLimits.xMax) / 2 ;
            m_roiButtonLimits.xMax += m_roiButtonLimits.xMin;
            m_roiButtonLimits.yMin = 0 ;
        }

        //
        // Step 3.
        //      We want to grab a FFOV image from the camera if we can.  However we can only grab
        //      an image if the user has the camera in a non-triggered streaming state.
        if (! gCamera->triggering() &&
            (streamInterrupted ||  gCamera->streaming()))
        {

            //
            // Step 4
            //      If the camera is using Interleaved HDR, Put the camera in CAMERA_MODE hdr
            bool   wasUsingInterleavedHdr = false;
            float  oldHdrMode;
            if (API_SUCCESS (gCamera->getValue (FEATURE_GAIN_HDR, &oldHdrMode)))
            {
                if (oldHdrMode == FEATURE_GAIN_HDR_MODE_INTERLEAVED)
                {
                    wasUsingInterleavedHdr = true;
                    gCamera->setValue (FEATURE_GAIN_HDR, FEATURE_GAIN_HDR_MODE_CAMERA);
                }
            }

            //
            // Step 5
            //      Adjust the ROI if necessary
            bool rioAdjustmentNecessary = m_roi != m_maxRoi;
            bool wasPreviewing = false;
            if (rioAdjustmentNecessary)
            {
                PXL_ROI ffov = m_maxRoi;
                wasPreviewing = gCamera->previewing();
                if (wasPreviewing) gCamera->pausePreview();
                rc = gCamera->setRoiValue(FrameRoi, ffov);

            }

            //
            // Step 6
            //      If the stream was temporarily disabled, turn it on
            if (streamInterrupted) gCamera->startStream();

            //
            // Step 7
            //      Grab and format the image
            std::vector<U8> frameBuf (gCamera->imageSizeInBytes());
            FRAME_DESC      frameDesc;
            std::vector<U8> rgbBuffer;
            U8*             pRgbBuffer;

            rc = gCamera->getNextFrame(frameBuf.size(), &frameBuf[0], &frameDesc);
            if (API_SUCCESS(rc))
            {

                // We need to display a RGB24_NON_DIB image, so convert it to this format, unless it already IS this format
                if (frameDesc.PixelFormat.fValue == PIXEL_FORMAT_RGB24_NON_DIB)
                {
                    pRgbBuffer = &frameBuf[0];
                } else {

                    // Allocate an rgb Buffer.  the rgbBuffer may be bigger or smaller than the frame buffer, depending on
                    // the pixel format.  Play it safe, and deal with the worst case where we need 3 bytes for every pixel
                    rgbBuffer.resize (frameBuf.size()*3);
                    pRgbBuffer = &rgbBuffer[0];
                    rc = gCamera->formatRgbImage(&frameBuf[0], &frameDesc, rgbBuffer.size(), pRgbBuffer);
                }

                if (API_SUCCESS(rc))
                {
                    //
                    // Step 8
                    //      Convert the rgb image into a ffov buffer.  Be sure to use the same aspect ratio as the imager


                    GdkPixbuf  *tempPixBuf = gdk_pixbuf_new_from_data (
                            pRgbBuffer, GDK_COLORSPACE_RGB, false, 8,
                            (int)(frameDesc.Roi.fWidth / frameDesc.PixelAddressingValue.fHorizontal),
                            (int)(frameDesc.Roi.fHeight / frameDesc.PixelAddressingValue.fVertical),
                            (int)(frameDesc.Roi.fWidth / frameDesc.PixelAddressingValue.fHorizontal)*3, NULL, NULL);
                    m_ffovBuf = gdk_pixbuf_scale_simple (tempPixBuf,
                            m_roiButtonLimits.xMax - m_roiButtonLimits.xMin,
                            m_roiButtonLimits.yMax - m_roiButtonLimits.yMin,
                            GDK_INTERP_BILINEAR);
                    gtk_image_set_from_pixbuf (GTK_IMAGE (m_ffovImage), m_ffovBuf);
                }
            }

            //
            // Step 9
            //      Restore the ROI, preview, and stream as necessary
            if (streamInterrupted) gCamera->stopStream ();
            if (rioAdjustmentNecessary)
            {
                gCamera->setRoiValue(FrameRoi, m_roi);
                if (wasPreviewing) gCamera->playPreview();
            }

            //
            // Step 10
            //      If the camera was using Interleaved HDR, re-enable it
            if (wasUsingInterleavedHdr)
            {
                gCamera->setValue (FEATURE_GAIN_HDR, oldHdrMode);
            }
        } else {

            //
            // Step 11
            //      We can't grab an image.  Display the No Stream FFOV image (if we're not
            //      already displaying it
            if (m_ffovBuf == NULL)
            {
                m_ffovBuf = gdk_pixbuf_new_from_file_at_scale ("NoStream.bmp",
                        m_roiButtonLimits.xMax - m_roiButtonLimits.xMin,
                        m_roiButtonLimits.yMax - m_roiButtonLimits.yMin
                        , false, NULL);
                gtk_image_set_from_pixbuf (GTK_IMAGE (m_ffovImage), m_ffovBuf);
            }
        }
    }
}

// This will update the ROI button so that it reflects the current ROI.  Assumes m_roi and m_maxRoi are valid
void PxLStream::updateRoiButton()
{
    // The button should only be visible if we have a camera
    if (gCamera)
    {
        gtk_widget_show (m_roiButton);

        float widthPercent =   (float)m_roi.m_width / (float)m_maxRoi.m_width;
        float heightPercent =  (float)m_roi.m_height / (float)m_maxRoi.m_height;
        float offsetXPercent = (float)m_roi.m_offsetX / (float)m_maxRoi.m_width;
        float offsetYPercent = (float)m_roi.m_offsetY / (float)m_maxRoi.m_height;

        float xLimit = (float)(m_roiButtonLimits.xMax - m_roiButtonLimits.xMin);
        float yLimit = (float)(m_roiButtonLimits.yMax - m_roiButtonLimits.yMin);

        m_roiButtonRoi.m_width =  (int)(xLimit * widthPercent);
        m_roiButtonRoi.m_height = (int)(yLimit * heightPercent);
        m_roiButtonRoi.m_offsetX =  (int)(xLimit * offsetXPercent) + m_roiButtonLimits.xMin;
        m_roiButtonRoi.m_offsetY =  (int)(yLimit * offsetYPercent) + m_roiButtonLimits.yMin;
        setRoiButtonSize ();
        // Bugzilla.1304 -- This deprecated function is necessary because Ubuntu 14.04 on ARM uses an old library
        gtk_widget_set_margin_left (m_roiButton, m_roiButtonRoi.m_offsetX);
        gtk_widget_set_margin_top (m_roiButton, m_roiButtonRoi.m_offsetY);
    } else {
        gtk_widget_hide (m_roiButton);
    }
}

// Returns the operatation type for a given mouse position
ROI_BUTTON_OPS PxLStream::determineOperationFromMouse (double relativeX, double relativeY)
{
    if (relativeY <= m_EdgeSensitivity)
    {
        if (relativeX <= m_EdgeSensitivity) return OP_RESIZE_TOP_LEFT;
        if (relativeX >= m_roiButtonRoi.m_width-m_EdgeSensitivity) return OP_RESIZE_TOP_RIGHT;
        return OP_RESIZE_TOP;
    } else if (relativeY >= m_roiButtonRoi.m_height-m_EdgeSensitivity) {
        if (relativeX <= m_EdgeSensitivity) return OP_RESIZE_BOTTOM_LEFT;
        if (relativeX >= m_roiButtonRoi.m_width-m_EdgeSensitivity) return OP_RESIZE_BOTTOM_RIGHT;
        return OP_RESIZE_BOTTOM;
    } else if (relativeX <= m_EdgeSensitivity) {
        return OP_RESIZE_LEFT;
    } else if (relativeX >= m_roiButtonRoi.m_width-m_EdgeSensitivity) {
        return OP_RESIZE_RIGHT;
    } else return OP_MOVE;

}

// Mouse down on the ROI button will start an operation
void PxLStream::startRoiButtonOperation( GtkWidget *widget, double relativeX, double relativeY, double absoluteX, double absoluteY)
{
    m_mouseXOnOpStart = absoluteX;
    m_mouseYOnOpStart = absoluteY;

    m_roiButtonOnOpStart = m_roiButtonRoi;

    m_currentOp = determineOperationFromMouse (relativeX, relativeY);

}

// Mouse up on the ROI button will finish an operation
void PxLStream::finishRoiButtonOperation( GtkWidget *widget)
{
    bool bPreviewRestoreRequired = false;
    bool bStreamRestoreRequired = false;

    //
    // Step 1
    //      Has the operation resulted in a new ROI?  That will only be true if the user has moved
    //      mouse sufficiently while in a mouse down state
    if (abs (m_roiButtonRoi.m_width   - m_roiButtonOnOpStart.m_width)   >= m_EdgeSensitivity ||
        abs (m_roiButtonRoi.m_height  - m_roiButtonOnOpStart.m_height)  >= m_EdgeSensitivity ||
        abs (m_roiButtonRoi.m_offsetX - m_roiButtonOnOpStart.m_offsetX) >= m_EdgeSensitivity ||
        abs (m_roiButtonRoi.m_offsetY - m_roiButtonOnOpStart.m_offsetY) >= m_EdgeSensitivity)
    {
        //
        // Step 2
        //      Figure out what ROI user wants based on the current size and position of the ROI window
        float xLimit = (float)(m_roiButtonLimits.xMax - m_roiButtonLimits.xMin);
        float yLimit = (float)(m_roiButtonLimits.yMax - m_roiButtonLimits.yMin);

        float widthPercent =   (float)m_roiButtonRoi.m_width / xLimit;
        float heightPercent =  (float)m_roiButtonRoi.m_height / yLimit;
        float offsetXPercent = (float)(m_roiButtonRoi.m_offsetX - m_roiButtonLimits.xMin) / xLimit;
        float offsetYPercent = (float)(m_roiButtonRoi.m_offsetY - m_roiButtonLimits.yMin) / yLimit;

        //
        //  Step 3
        //      Rounding is going to occur because of the scaling we use for the ROI (button) representation.  However,
        //      limit those rounding errors to just the dimensions that the user is adjusting
        PXL_ROI newRoi = m_roi;
        switch (m_currentOp)
        {
        case OP_MOVE:
            newRoi.m_offsetX =  (int)((float)m_maxRoi.m_width * offsetXPercent);
            newRoi.m_offsetY =  (int)((float)m_maxRoi.m_height * offsetYPercent);
            break;
        case OP_RESIZE_TOP_LEFT:
            newRoi.m_width =  (int)((float)m_maxRoi.m_width * widthPercent);
            newRoi.m_height = (int)((float)m_maxRoi.m_height * heightPercent);
            newRoi.m_offsetX =  (int)((float)m_maxRoi.m_width * offsetXPercent);
            newRoi.m_offsetY =  (int)((float)m_maxRoi.m_height * offsetYPercent);
            break;
        case OP_RESIZE_TOP_RIGHT:
            newRoi.m_width =  (int)((float)m_maxRoi.m_width * widthPercent);
            newRoi.m_height = (int)((float)m_maxRoi.m_height * heightPercent);
            newRoi.m_offsetX =  (int)((float)m_maxRoi.m_width * offsetXPercent);
            break;
        case OP_RESIZE_BOTTOM_RIGHT:
            newRoi.m_width =  (int)((float)m_maxRoi.m_width * widthPercent);
            newRoi.m_height = (int)((float)m_maxRoi.m_height * heightPercent);
            break;
        case OP_RESIZE_BOTTOM_LEFT:
            newRoi.m_width =  (int)((float)m_maxRoi.m_width * widthPercent);
            newRoi.m_height = (int)((float)m_maxRoi.m_height * heightPercent);
            newRoi.m_offsetX =  (int)((float)m_maxRoi.m_width * offsetXPercent);
            break;
        case OP_RESIZE_TOP:
            newRoi.m_height = (int)((float)m_maxRoi.m_height * heightPercent);
            newRoi.m_offsetY =  (int)((float)m_maxRoi.m_height * offsetYPercent);
            break;
        case OP_RESIZE_RIGHT:
            newRoi.m_width =  (int)((float)m_maxRoi.m_width * widthPercent);
            break;
        case OP_RESIZE_BOTTOM:
            newRoi.m_height = (int)((float)m_maxRoi.m_height * heightPercent);
            break;
        case OP_RESIZE_LEFT:
            newRoi.m_width =  (int)((float)m_maxRoi.m_width * widthPercent);
            newRoi.m_offsetX =  (int)((float)m_maxRoi.m_width * offsetXPercent);
            break;
        case OP_NONE:
        default:
            break;
        }

        //
        // Step 4
        //      Set this ROI in the camera.  Note that the camera may make an adjustment to our ROI

        PxLAutoLock lock(&gCameraLock);

        bPreviewRestoreRequired = gCamera->previewing();
        bStreamRestoreRequired = gCamera->streaming() && ! gCamera->triggering();
        if (bPreviewRestoreRequired) gCamera->pausePreview();
        if (bStreamRestoreRequired)gCamera->stopStream();


        PXL_RETURN_CODE rc = gCamera->setRoiValue(FrameRoi, newRoi);
        if (API_SUCCESS(rc))
        {
            if (rc == ApiSuccessParametersChanged)
            {
                // The camera had to adjust the parameters to make them work, read it back to get the correct values.
                rc = gCamera->getRoiValue(FrameRoi, &newRoi);
            }
        }
        if (API_SUCCESS(rc))
        {
            m_roi = newRoi;
        }

        //
        // Step 5
        //      Update our controls to indicate we are using the new ROI

        // Step 5.1
        //      Update the ROI edits
        char cValue[40];

        sprintf (cValue, "%d",newRoi.m_offsetX);
        gtk_entry_set_text (GTK_ENTRY (m_offsetX), cValue);
        sprintf (cValue, "%d",newRoi.m_offsetY);
        gtk_entry_set_text (GTK_ENTRY (m_offsetY), cValue);
        sprintf (cValue, "%d",newRoi.m_width);
        gtk_entry_set_text (GTK_ENTRY (m_width), cValue);
        sprintf (cValue, "%d",newRoi.m_height);
        gtk_entry_set_text (GTK_ENTRY (m_height), cValue);

        //
        // Step 5.2
        //      Update the ROI dropdown

        // Temporarily block the handler for ROI selection
        g_signal_handlers_block_by_func (gStreamTab->m_roiCombo, (gpointer)NewRoiSelected, NULL);

        // If we are no longer using the 'custom' ROI at the end of our dropdown, remove it.
        if (m_usingNonstandardRoi)
        {
            gtk_combo_box_text_remove (GTK_COMBO_BOX_TEXT(gStreamTab->m_roiCombo),
                                       --gStreamTab->m_numRoisSupported);
            gStreamTab->m_supportedRois.pop_back();
            gStreamTab->m_usingNonstandardRoi = false;
        }

        int newSelectionIndex;
        std::vector<PXL_ROI>::iterator it;
        it = find (gStreamTab->m_supportedRois.begin(), gStreamTab->m_supportedRois.end(), newRoi);
        if (it == gStreamTab->m_supportedRois.end())
        {
            // This is a new 'non standard' ROI.  Add it to the end
            newSelectionIndex = gStreamTab->m_numRoisSupported;
            sprintf (cValue, "%d x %d", newRoi.m_width, newRoi.m_height);
            gtk_combo_box_text_insert_text (GTK_COMBO_BOX_TEXT(gStreamTab->m_roiCombo),
                                            gStreamTab->m_numRoisSupported++,
                                            cValue);
            gStreamTab->m_supportedRois.push_back(newRoi);
            gStreamTab->m_usingNonstandardRoi = true;
        } else {
            // the user 'created' a standard ROI
            newSelectionIndex = it - gStreamTab->m_supportedRois.begin();
        }
        gtk_combo_box_set_active (GTK_COMBO_BOX(gStreamTab->m_roiCombo),newSelectionIndex);
        // Unblock the handler for ROI selection
        g_signal_handlers_unblock_by_func (gStreamTab->m_roiCombo, (gpointer)NewRoiSelected, NULL);

        // Step 5
        //      Adjust the preview size if necessary
        if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON(gVideoPreviewTab->m_autoResize)))
        {
            rc = gCamera->resizePreviewToRoi();
        }

        // Step 6
        //      Let interested tabs know that that the ROI has changed
        gControlsTab->refreshRequired(false); // Bugzilla.1613
#if defined VIDEO_CAPTURE_SUPPORTED
        gVideoTab->refreshRequired(false);
#endif
        gLensTab->refreshRequired(false);
        gAutoRoiTab->refreshRequired(false);
    }

    // Reflect these adjustments in the ROI button
    loadFfovImage (bStreamRestoreRequired);
    updateRoiButton();
    if (bStreamRestoreRequired) gCamera->startStream();
    if (bPreviewRestoreRequired) gCamera->playPreview();

    m_currentOp = OP_NONE;
}

// Set the size of the ROI button, including putting in the label if there is room
void PxLStream::setRoiButtonSize ()
{
    bool bLabel;  // true if we want the ROI to have a label

     bLabel = (m_roiButtonRoi.m_width >= ROI_BUTTON_LABEL_THRESHOLD_WIDTH) &&
              (m_roiButtonRoi.m_height >= ROI_BUTTON_LABEL_THRESHOLD_HEIGHT);
     gtk_button_set_label (GTK_BUTTON(m_roiButton), bLabel ? RoiLabel: "");
     gtk_widget_set_size_request (m_roiButton, m_roiButtonRoi.m_width, m_roiButtonRoi.m_height);
}

/* ---------------------------------------------------------------------------
 * --   gtk thread callbacks - used to update controls
 * ---------------------------------------------------------------------------
 */

// Indicate that the refresh is no longer outstanding, it has completed.
static gboolean RefreshComplete (gpointer pData)
{
    PxLStream *pStream = (PxLStream *)pData;

    // Update the ROI button
    //
    // 2018-02-06
    //    As an optimization, don't update it here after everything is done.  It should
    //    have already been updated (via ROiActivate).
    //pStream->loadFfovImage();
    //pStream->updateRoiButton();

    pStream->m_numRefreshRequestsOutstanding--;
    return false;
}

//
// Make ROI meaningless
static gboolean RoiDeactivate (gpointer pData)
{
    PxLStream *pStream = (PxLStream *)pData;

    gtk_widget_set_sensitive (pStream->m_roiCombo, false);
    gtk_widget_set_sensitive (pStream->m_offsetX, false);
    gtk_widget_set_sensitive (pStream->m_offsetY, false);
    gtk_widget_set_sensitive (pStream->m_width, false);
    gtk_widget_set_sensitive (pStream->m_height, false);
    gtk_widget_set_sensitive (pStream->m_center, false);

    pStream->m_ffovBuf = NULL; // Don't use the current FFOV image

    pStream->loadFfovImage();
    pStream->updateRoiButton();

    return false;  //  Only run once....
}

//
// Assert all of the ROI controls
static gboolean RoiActivate (gpointer pData)
{
    PxLStream *pStream = (PxLStream *)pData;

    bool settable = false;

    //
    // Step 0
    //      Read the default cursor.
    pStream->m_originalCursor = gdk_window_get_cursor (gtk_widget_get_window (pStream->m_ffovImage));

    PxLAutoLock lock(&gCameraLock);

    if (gCamera)
    {
        pStream->m_numRoisSupported = 0;
        pStream->m_supportedRois.clear();
        pStream->m_usingNonstandardRoi = false;
        gtk_combo_box_text_remove_all (GTK_COMBO_BOX_TEXT(pStream->m_roiCombo));

        if (gCamera->supported(FEATURE_ROI))
        {
            settable = gCamera->settable (FEATURE_ROI);

            PXL_ROI minRoi, maxRoi, currentRoi;
            PXL_RETURN_CODE rc = gCamera->getRoiValue (FrameRoi, &currentRoi);

            if (API_SUCCESS(rc))
            {
                char cValue[40];

                //
                // Step 1
                //      ROI Size and location edits
                sprintf (cValue, "%d",currentRoi.m_offsetX);
                gtk_entry_set_text (GTK_ENTRY (pStream->m_offsetX), cValue);
                sprintf (cValue, "%d",currentRoi.m_offsetY);
                gtk_entry_set_text (GTK_ENTRY (pStream->m_offsetY), cValue);
                sprintf (cValue, "%d",currentRoi.m_width);
                gtk_entry_set_text (GTK_ENTRY (pStream->m_width), cValue);
                sprintf (cValue, "%d",currentRoi.m_height);
                gtk_entry_set_text (GTK_ENTRY (pStream->m_height), cValue);

                //
                // Step 2
                //      build the ROI drop down.   This is more involved

                // Step 2.1
                //      Interrupt the stream
                {
                    bool restoreRequired = false;
                    // 2018-02-06
                    //    As an optimiations, given that we are not 'trying' any of the standard ROIs, we dont
                    //    need to stop the stream
                    //TEMP_STREAM_STOP();

                    // Step 2.2
                    //      Add all of the standard ROIS that are larger than min, and smaller than max, and are supported
                    //      by the camera
                    rc = gCamera->getRoiRange (FrameRoi, &minRoi, &maxRoi);
                    maxRoi.m_offsetX = maxRoi.m_offsetY = 0;  // Offests are 0 for ffov
                    if (API_SUCCESS(rc))
                    {
                        // record the maxRoi, to avoid having to read it a bunch of times.
                        pStream->m_maxRoi = maxRoi;
                        pStream->m_cameraAspectRatio = (float)maxRoi.m_width / (float)maxRoi.m_height;

                        // Step 2.3
                        //      If we are rotating 90 or 270 degrees, we need to flip the 'standard' ROIs
                        bool  transpose = false;
                        float rotation = 0.0;
                        rc = gCamera->getValue (FEATURE_ROTATE, &rotation);
                        if (API_SUCCESS (rc) && (rotation == 90.0 || rotation == 270.0)) transpose = true;
                        for (int i=0; i < (int)PxLStandardRois.size(); i++)
                        {
                            if (PxLStandardRois[i] < minRoi) continue;
                            if (PxLStandardRois[i] > maxRoi) break;
                            //
                            // 2018-01-25
                            //     As an 'optimzation, don't bother checking each of the indivdual ROIs, simply add
                            //     each of the standard ones (that will fit).  This menas that the times in the list
                            //     might not be supported EXACTLY in the camera; the camera may need to adjust them
                            //     to make it fit.
                            //rc = gCamera->setRoiValue (FrameRoi, PxLStandardRois[i]);
                            if (API_SUCCESS(rc))
                            {
                                //if (PxLStandardRois[i] != currentRoi) restoreRequired = true;
                                PXL_ROI adjustedRoi = PxLStandardRois[i];
                                if (transpose) adjustedRoi.rotateClockwise(rotation);
                                //if (rc == ApiSuccessParametersChanged)
                                //{
                                //    rc = gCamera->getRoiValue (FrameRoi, &adjustedRoi);
                                //}
                                if (API_SUCCESS(rc))
                                {
                                    sprintf (cValue, "%d x %d", adjustedRoi.m_width, adjustedRoi.m_height);
                                    gtk_combo_box_text_insert_text (GTK_COMBO_BOX_TEXT(pStream->m_roiCombo),
                                                                    pStream->m_numRoisSupported++,
                                                                    cValue);
                                    pStream->m_supportedRois.push_back(adjustedRoi);
                                }
                            }
                        }

                        // Step 2.3
                        //      If it's not already there, add the max ROI to the end of the list
                        if (pStream->m_numRoisSupported == 0 || pStream->m_supportedRois[pStream->m_numRoisSupported-1] != maxRoi)
                        {
                            sprintf (cValue, "%d x %d", maxRoi.m_width, maxRoi.m_height);
                            gtk_combo_box_text_insert_text (GTK_COMBO_BOX_TEXT(pStream->m_roiCombo),
                                                            pStream->m_numRoisSupported++,
                                                            cValue);
                            pStream->m_supportedRois.push_back(maxRoi);
                        }

                        // Step 2.4
                        //      If the current ROI isn't one of the standard one from the list, add this 'custom' one
                        //      to the end.
                        if (find (pStream->m_supportedRois.begin(),
                                  pStream->m_supportedRois.end(),
                                  currentRoi) == pStream->m_supportedRois.end())
                        {
                            sprintf (cValue, "%d x %d", currentRoi.m_width, currentRoi.m_height);
                            gtk_combo_box_text_insert_text (GTK_COMBO_BOX_TEXT(pStream->m_roiCombo),
                                                            pStream->m_numRoisSupported++,
                                                            cValue);
                            pStream->m_supportedRois.push_back(currentRoi);
                            pStream->m_usingNonstandardRoi = true;
                        }

                        // Step 2.5
                        //      If we changed the ROI, restore the original one
                        if (restoreRequired)
                        {
                            rc = gCamera->setRoiValue (FrameRoi, currentRoi);
                        }
                    }

                    // Step 3
                    //      Mark the current ROI as 'selected'
                    for (int i=0; i<(int)pStream->m_supportedRois.size(); i++)
                    {
                        if (pStream->m_supportedRois[i] != currentRoi) continue;
                        gtk_combo_box_set_active (GTK_COMBO_BOX(pStream->m_roiCombo),i);
                        break;
                    }

                    // Step 4
                    //      Remember the current ROI
                    pStream->m_roi = currentRoi;

                }
            }
        }
    }

    // Update the FFOV image and ROI button
    pStream->loadFfovImage();
    pStream->updateRoiButton ();

    // ROI controls
    gtk_widget_set_sensitive (pStream->m_offsetX, settable);
    gtk_widget_set_sensitive (pStream->m_offsetY, settable);
    gtk_widget_set_sensitive (pStream->m_width, settable);
    gtk_widget_set_sensitive (pStream->m_height, settable);

    gtk_widget_set_sensitive (pStream->m_center, settable);

    //ROI drop-down
    gtk_widget_set_sensitive (pStream->m_roiCombo, settable);

    return false;  //  Only run once....
}

//
// Make pixelformat meaningless
static gboolean PixelFormatDeactivate (gpointer pData)
{
    PxLStream *pStream = (PxLStream *)pData;

    gtk_widget_set_sensitive (pStream->m_pixelFormat, false);

    return false;  //  Only run once....
}

//
// Assert all of the pixel format controls
static gboolean PixelFormatActivate (gpointer pData)
{
    PxLStream *pStream = (PxLStream *)pData;

    bool settable = false;

    PxLAutoLock lock(&gCameraLock);

    if (gCamera)
    {
        if (gCamera->supported(FEATURE_PIXEL_FORMAT))
        {
            float min, max, currentValue;
            bool restoreRequired = false;
            PXL_RETURN_CODE rc;

            gCamera->getRange(FEATURE_PIXEL_FORMAT, &min, &max);
            settable = min != max;
            gCamera->getValue(FEATURE_PIXEL_FORMAT, &currentValue);
            pStream->m_supportedPixelFormats.clear();
            gtk_combo_box_text_remove_all (GTK_COMBO_BOX_TEXT(pStream->m_pixelFormat));

            //
            // The most straight forward way of identifying all of the supported pixel formats, it to try
            // each pixel format between min and max (we know min and max ARE supported).  However, that
            // takes a while.  To try to speed things up, as are going to try to do this a little more intelligently.
            // More specifically, we assume:
            //    1. A camera is either mono or color
            //    2. A camera is either a polar camera or not (not == normal)
            //    3. Polar mono cameras support
            //        o PIXEL_FORMAT_STOKES4_12
            //        o PIXEL_FORMAT_POLAR4_12
            //        o PIXEL_FORMAT_POLAR_RAW4_12
            //        o PIXEL_FORMAT_HSV4_12
            //    4. All non-polar mono cameras support PIXEL_FORMAT_MONO8
            //    5. All non-polar color cameras support PIXEL_FORMAT_BAYER8
            //    6. Non-polar Mono cameras optionally support
            //        o PIXEL_FORMAT_MONO16
            //        o PIXEL_FORMAT_MONO12_PACKED or PIXEL_FORMAT_MONO12_PACKED_MSFIRST (but not both)
            //        o PIXEL_FORMAT_MONO10_PACKED_MSFIRST
            //    7. Non-polar Color cameras optionally support
            //        o PIXEL_FORMAT_BAYER16
            //        o PIXEL_FORMAT_YUV422
            //        o PIXEL_FORMAT_BAYER12_PACKED or FORMAT_BAYER12_PACKED_MSFIRST (but not both)
            //        o PIXEL_FORMAT_BAYER10_PACKED_MSFIRST
            //        o PIXEL_FORMAT_RGB24_NON_DIB
            //        o PIXEL_FORMAT_BGR24_NON_DIB
            //        o PIXEL_FORMAT_RGBA
            //        o PIXEL_FORMAT_BGRA
            //        o PIXEL_FORMAT_ARGB
            //        o PIXEL_FORMAT_ABGR


            std::vector<COEM_PIXEL_FORMATS> optionalformats;
            if (min == PIXEL_FORMAT_MONO8)
            {
                // Non-Polar Mono camera.

                // We know mono camera support mono8
                gtk_combo_box_text_insert_text (
                        GTK_COMBO_BOX_TEXT(pStream->m_pixelFormat),
                        MONO8,
                        PxLFormatStrings[MONO8]);
                pStream->m_supportedPixelFormats.push_back (MONO8);

                optionalformats.push_back(MONO10_PACKED);
                optionalformats.push_back(MONO12_PACKED);
                optionalformats.push_back(MONO16);
            } else if (min == PIXEL_FORMAT_STOKES4_12) {
                // Polar mono camera
                gtk_combo_box_text_insert_text (
                        GTK_COMBO_BOX_TEXT(pStream->m_pixelFormat),
                        STOKES,
                        PxLFormatStrings[STOKES]);
                gtk_combo_box_text_insert_text (
                        GTK_COMBO_BOX_TEXT(pStream->m_pixelFormat),
                        POLAR,
                        PxLFormatStrings[POLAR]);
                gtk_combo_box_text_insert_text (
                        GTK_COMBO_BOX_TEXT(pStream->m_pixelFormat),
                        POLAR_RAW,
                        PxLFormatStrings[POLAR_RAW]);
                gtk_combo_box_text_insert_text (
                        GTK_COMBO_BOX_TEXT(pStream->m_pixelFormat),
                        HSV,
                        PxLFormatStrings[HSV]);
                pStream->m_supportedPixelFormats.push_back (STOKES);
                pStream->m_supportedPixelFormats.push_back (POLAR);
                pStream->m_supportedPixelFormats.push_back (POLAR_RAW);
                pStream->m_supportedPixelFormats.push_back (HSV);
            } else {
                // Non-Polar Color camera.

                // We know color camera support bayer8
                gtk_combo_box_text_insert_text (
                        GTK_COMBO_BOX_TEXT(pStream->m_pixelFormat),
                        BAYER8,
                        PxLFormatStrings[BAYER8]);
                pStream->m_supportedPixelFormats.push_back (BAYER8);

                optionalformats.push_back(BAYER10_PACKED);
                optionalformats.push_back(BAYER12_PACKED);
                optionalformats.push_back(BAYER16);
                optionalformats.push_back(YUV422);
                optionalformats.push_back(RGB24);
                optionalformats.push_back(BGR24);
                optionalformats.push_back(RGBA);
                optionalformats.push_back(BGRA);
                optionalformats.push_back(ARGB);
                optionalformats.push_back(ABGR);
            }

            //      Temporarily stop the stream so that we can make some PF adjustment
            TEMP_STREAM_STOP();

            for (int i=0; i < (int)optionalformats.size(); i++)
            {
                rc = gCamera->setValue(FEATURE_PIXEL_FORMAT, PxLPixelFormat::toApi(optionalformats[i]));
                if (API_SUCCESS(rc))
                {
                    restoreRequired = true;
                    gtk_combo_box_text_insert_text (
                            GTK_COMBO_BOX_TEXT(pStream->m_pixelFormat),
                            i+1,
                            PxLFormatStrings[optionalformats[i]]);
                    pStream->m_supportedPixelFormats.push_back (optionalformats[i]);
                }
            }
            // restore the old value (if necessary)
            if (restoreRequired) gCamera->setValue(FEATURE_PIXEL_FORMAT, currentValue);

            // And finally, show our current pixel format as the 'active' one
            for (int i=0; i<(int)pStream->m_supportedPixelFormats.size(); i++)
            {
                if (pStream->m_supportedPixelFormats[i] != PxLPixelFormat::fromApi (currentValue)) continue;
                gtk_combo_box_set_active (GTK_COMBO_BOX(pStream->m_pixelFormat),i);
                break;
            }
        }
    }

    gtk_widget_set_sensitive (pStream->m_pixelFormat, settable);

    return false;  //  Only run once....
}

//
// Make compression meaningless
static gboolean CompressionDeactivate (gpointer pData)
{
    PxLStream *pStream = (PxLStream *)pData;

    gtk_widget_set_sensitive (pStream->m_compression, false);

    return false;  //  Only run once....
}

//
// Assert all of the compression info
static gboolean CompressionActivate (gpointer pData)
{
    PxLStream *pStream = (PxLStream *)pData;

    bool supported = false;
    bool enabled = false;

    PxLAutoLock lock(&gCameraLock);

    if (gCamera)
    {
        if (gCamera->supported(FEATURE_COMPRESSION))
        {
            float compressionParams[2];
            PXL_RETURN_CODE rc;
            
            // Get the current pixel format
            rc = gCamera->getValue(FEATURE_PIXEL_FORMAT, compressionParams);

            if (API_SUCCESS(rc) && eightBitFormat(compressionParams[0]))
            {
                // Yup -- this format supports it.  Update the toggle to the current value
                supported = true;
                rc = gCamera->get2ndValue (FEATURE_COMPRESSION, compressionParams[0], &compressionParams[1]);

                enabled = (compressionParams[1] != FEATURE_COMPRESSION_STRATEGY_NONE);
            }
        }
    }

    gtk_widget_set_sensitive (pStream->m_compression, supported);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(pStream->m_compression), supported && enabled);

    return false;  //  Only run once....
}

//
// Make pixelformatInterpretation meaningless
static gboolean PixelFormatInterpretationDeactivate (gpointer pData)
{
    PxLStream *pStream = (PxLStream *)pData;

    gtk_widget_set_sensitive (pStream->m_pixelFormatInterpretation, false);

    return false;  //  Only run once....
}

//
// Assert all of the pixel format interpretations
static gboolean PixelFormatInterpretationActivate (gpointer pData)
{
    PxLStream *pStream = (PxLStream *)pData;

    bool settable = false;

    PxLAutoLock lock(&gCameraLock);

    if (gCamera)
    {
        if (gCamera->supported(FEATURE_PIXEL_FORMAT))
        {
            float currentValue;
            PXL_RETURN_CODE rc;

            pStream->m_supportedPixelFormatInterpretations.clear();
            gtk_combo_box_text_remove_all (GTK_COMBO_BOX_TEXT(pStream->m_pixelFormatInterpretation));
            rc = gCamera->getValue(FEATURE_PIXEL_FORMAT, &currentValue);

            if (API_SUCCESS(rc) && currentValue == PIXEL_FORMAT_HSV4_12)
            {
                // Polar mono camera
                gtk_combo_box_text_insert_text (
                        GTK_COMBO_BOX_TEXT(pStream->m_pixelFormatInterpretation),
                        HSV_AS_COLOR,
                        PxLFormatInterpretationStrings[HSV_AS_COLOR]);
                gtk_combo_box_text_insert_text (
                        GTK_COMBO_BOX_TEXT(pStream->m_pixelFormatInterpretation),
                        HSV_AS_ANGLE,
                        PxLFormatInterpretationStrings[HSV_AS_ANGLE]);
                gtk_combo_box_text_insert_text (
                        GTK_COMBO_BOX_TEXT(pStream->m_pixelFormatInterpretation),
                        HSV_AS_DEGREE,
                        PxLFormatInterpretationStrings[HSV_AS_DEGREE]);
                pStream->m_supportedPixelFormatInterpretations.push_back (HSV_AS_COLOR);
                pStream->m_supportedPixelFormatInterpretations.push_back (HSV_AS_ANGLE);
                pStream->m_supportedPixelFormatInterpretations.push_back (HSV_AS_DEGREE);

                // Determine how this camera is interpretating the pixel format
                COEM_PIXEL_FORMAT_INTERPRETATIONS interpretation = HSV_AS_COLOR;
                if (API_SUCCESS (gCamera->getValue(FEATURE_POLAR_HSV_INTERPRETATION, &currentValue)))
                {
                    interpretation = (COEM_PIXEL_FORMAT_INTERPRETATIONS)currentValue;
                }
                gtk_combo_box_set_active (GTK_COMBO_BOX(pStream->m_pixelFormatInterpretation),interpretation);

                settable = true;
            }
        }
    }

    gtk_widget_set_sensitive (pStream->m_pixelFormatInterpretation, settable);

    return false;  //  Only run once....
}

//
// Make alpha meaningless
static gboolean AlphaDeactivate (gpointer pData)
{
    PxLStream *pStream = (PxLStream *)pData;

    gtk_widget_set_sensitive (pStream->m_alpha, false);

    return false;  //  Only run once....
}

//
// Assert alpha value, if it is usig apixel format with an alpha value
static gboolean AlphaActivate (gpointer pData)
{
    PxLStream *pStream = (PxLStream *)pData;

    bool settable = false;

    PxLAutoLock lock(&gCameraLock);

    if (gCamera)
    {
        if (gCamera->supported(FEATURE_PIXEL_FORMAT))
        {
            int currentPixelFormat = -1;
            ULONG currentAlphaValue = 0;
            PXL_RETURN_CODE rc;

            rc = gCamera->getAlpha(&currentPixelFormat, &currentAlphaValue);

            if (API_SUCCESS(rc) && currentPixelFormat != -1)
            {
                // The camera IS using a alpha pixel format.
                char cValue[40];

                sprintf (cValue, "%d",currentAlphaValue);
                gtk_entry_set_text (GTK_ENTRY (pStream->m_alpha), cValue);


                settable = true;
            }
        }
    }

    gtk_widget_set_sensitive (pStream->m_alpha, settable);

    return false;  //  Only run once....
}

//
// Make pixel addressing meaningless
static gboolean PixelAddressingDeactivate (gpointer pData)
{
    PxLStream *pStream = (PxLStream *)pData;

    gtk_widget_set_sensitive (pStream->m_pixelAddressingMode, false);
    gtk_widget_set_sensitive (pStream->m_pixelAddressingValue, false);

    pStream->m_pixelAddressX = 1;
    pStream->m_pixelAddressX = 1;

    return false;  //  Only run once....
}

//
// Assert all of the pixel addressing controls
static gboolean PixelAddressingActivate (gpointer pData)
{
    PxLStream *pStream = (PxLStream *)pData;

    bool modeSettable = false;
    bool valueSettable = false;
    bool supportsAsymetricPA = false;

    PxLAutoLock lock(&gCameraLock);

    if (gCamera)
    {
        if (gCamera->supported(FEATURE_PIXEL_ADDRESSING))
        {
            float minMode, maxMode, currentMode;
            float minValue, maxValue, currentValueX, currentValueY;
            bool restoreRequired = false;
            PXL_RETURN_CODE rc;

            //
            // Step 1
            //      Get the range of supported modes and values, as well as the current value
            rc = gCamera->getPixelAddressRange(&minMode, &maxMode, &minValue, &maxValue, &supportsAsymetricPA);
            if (API_SUCCESS(rc))
            {
                pStream->m_supportedPixelAddressModes.clear();
                pStream->m_supportedPixelAddressValues.clear();
                gtk_combo_box_text_remove_all (GTK_COMBO_BOX_TEXT(pStream->m_pixelAddressingMode));
                gtk_combo_box_text_remove_all (GTK_COMBO_BOX_TEXT(pStream->m_pixelAddressingValue));

                modeSettable = minMode != maxMode;
                valueSettable = minValue != maxValue;
                rc = gCamera->getPixelAddressValues (&currentMode, &currentValueX, &currentValueY);
                if (API_SUCCESS (rc))
                {
                    //
                    // Step 2
                    //      Remember these
                    pStream->m_pixelAddressX = (int)currentValueX;
                    pStream->m_pixelAddressY = (int)currentValueY;

                    //
                    // Step 3
                    //      Temporarily stop the stream so that we can make some PA adjustment
                    TEMP_STREAM_STOP();

                    //
                    // Step 4
                    //      Which PA modes are supported?
                    //
                    // Step 4.1
                    //      We know the min is supported, no need to 'try' it
                    gtk_combo_box_text_insert_text (
                            GTK_COMBO_BOX_TEXT(pStream->m_pixelAddressingMode),
                            PxLPixelAddress::modeFromApi(minMode),
                            PxLAddressModes[PxLPixelAddress::modeFromApi(minMode)]);
                    pStream->m_supportedPixelAddressModes.push_back (PxLPixelAddress::modeFromApi(minMode));
                    //
                    // Step 4.2
                    //      Walk through each mode between min and max, to see if it is supported
                    for (float candidate = minMode+1.0; candidate < maxMode; candidate+=1.0)
                    {
                        if (candidate != currentMode)
                        {
                            rc = gCamera->setPixelAddressValues(candidate, currentValueX, currentValueY);
                            restoreRequired = true;
                        }
                        if (API_SUCCESS(rc)) {
                            gtk_combo_box_text_insert_text (
                                    GTK_COMBO_BOX_TEXT(pStream->m_pixelAddressingMode),
                                    PxLPixelAddress::modeFromApi(candidate),
                                    PxLAddressModes[PxLPixelAddress::modeFromApi(candidate)]);
                            pStream->m_supportedPixelAddressModes.push_back (PxLPixelAddress::modeFromApi( candidate));
                        }
                    }

                    //
                    // Step 4.3
                    //      We know the max is supported.
                    if (maxMode > minMode &&
                        find (pStream->m_supportedPixelAddressModes.begin(),
                              pStream->m_supportedPixelAddressModes.end(),
                              PxLPixelAddress::modeFromApi(maxMode)) == pStream->m_supportedPixelAddressModes.end())
                    {
                        gtk_combo_box_text_insert_text (
                                GTK_COMBO_BOX_TEXT(pStream->m_pixelAddressingMode),
                                PxLPixelAddress::modeFromApi(maxMode),
                                PxLAddressModes[PxLPixelAddress::modeFromApi(maxMode)]);
                        pStream->m_supportedPixelAddressModes.push_back (PxLPixelAddress::modeFromApi(maxMode));
                    }
                    //
                    // Step 5
                    //      Which PA Values are supported?  We build a set of integers that represents all supported
                    //      PA values
                    std::vector<float> supportedValues;

                    //
                    // Step 5.1
                    //      We know the min is supported (it will be 1)
                    supportedValues.push_back (minValue);
                    //
                    // Step 5.2
                    //      Walk through each mode between min and max, to see if it is supported
                    rc = ApiSuccess;
                    for (float candidate = minValue+1.0; candidate < maxValue; candidate+=1.0)
                    {

                        if (candidate != currentValueX)
                        {
                            rc = gCamera->setPixelAddressValues (currentMode, candidate, candidate);
                            restoreRequired = true;
                        }
                        // Use symmetric values for simplicity
                        if (API_SUCCESS(rc)) {
                            supportedValues.push_back (candidate);
                        }
                    }
                    //
                    // Step 5.3
                    //      We know the max is supported.
                    if (maxValue > minValue &&
                        find (supportedValues.begin(),
                              supportedValues.end(),
                              maxValue) == supportedValues.end())
                    {
                        supportedValues.push_back (maxValue);
                    }

                    //
                    // Step 6
                    //      Now that we know which PA values are supported, and if the camera supports asymmetric
                    //      pixel addressing, use this to build our PA value dropdown.
                    for (int x = 0; x<(int)supportedValues.size(); x++)
                    {
                        for (int y = 0; y<(int)supportedValues.size(); y++)
                        {
                            if (x != y && !supportsAsymetricPA) continue;
                            gtk_combo_box_text_insert_text (
                                    GTK_COMBO_BOX_TEXT(pStream->m_pixelAddressingValue),
                                    PxLPixelAddress::valueFromApi(supportedValues[x],supportedValues[y]),
                                    PxLAddressValues[PxLPixelAddress::valueFromApi(supportedValues[x],supportedValues[y])]);
                            pStream->m_supportedPixelAddressValues.push_back (PxLPixelAddress::valueFromApi(supportedValues[x],supportedValues[y]));
                        }
                    }

                    //
                    // Step 7
                    //      If we changed the pixel addressing, restore the original value
                    if (restoreRequired)
                    {
                        gCamera->setPixelAddressValues (currentMode, currentValueX, currentValueY);
                    }

                    // Step 7
                    //      Show our current pixel Addressing as the 'active' one
                    for(int i=0; i< (int)pStream->m_supportedPixelAddressModes.size(); i++)
                    {
                        if (pStream->m_supportedPixelAddressModes[i] != PxLPixelAddress::modeFromApi (currentMode)) continue;
                            gtk_combo_box_set_active (GTK_COMBO_BOX(pStream->m_pixelAddressingMode),i);
                            break;
                    }
                    COEM_PIXEL_ADDRESS_VALUES paValue = PxLPixelAddress::valueFromApi (currentValueX, currentValueY);
                    for (int i=0; i<(int)pStream->m_supportedPixelAddressValues.size(); i++)
                    {
                        if (pStream->m_supportedPixelAddressValues[i] != paValue) continue;
                        gtk_combo_box_set_active (GTK_COMBO_BOX(pStream->m_pixelAddressingValue),i);
                        break;
                    }
                }
            }
        }
    }

    gtk_widget_set_sensitive (pStream->m_pixelAddressingMode, modeSettable);
    gtk_widget_set_sensitive (pStream->m_pixelAddressingValue, valueSettable);

    return false;  //  Only run once....
}

//
// Make transformations meaningless
static gboolean TransformationsDeactivate (gpointer pData)
{
    PxLStream *pStream = (PxLStream *)pData;

    gtk_widget_set_sensitive (pStream->m_rotate, false);
    gtk_widget_set_sensitive (pStream->m_flip, false);

    pStream->m_rotateValue = 0;
    pStream->m_verticalFlip = false;
    pStream->m_horizontalFlip = false;


    return false;  //  Only run once....
}

//
// Assert all of the transformations controls
static gboolean TransformationsActivate (gpointer pData)
{
    PxLStream *pStream = (PxLStream *)pData;

    bool rotateSettable = false;
    bool flipSettable = false;

    PxLAutoLock lock(&gCameraLock);

    if (gCamera)
    {
        if (gCamera->supported(FEATURE_ROTATE))
        {
            // rotate dropdown
            rotateSettable = true;

            gtk_combo_box_text_remove_all (GTK_COMBO_BOX_TEXT(pStream->m_rotate));

            gtk_combo_box_text_insert_text (GTK_COMBO_BOX_TEXT(pStream->m_rotate), 0, "None");
            gtk_combo_box_text_insert_text (GTK_COMBO_BOX_TEXT(pStream->m_rotate), 1, "90 Degrees CW");
            gtk_combo_box_text_insert_text (GTK_COMBO_BOX_TEXT(pStream->m_rotate), 2, "180 Degrees CW");
            gtk_combo_box_text_insert_text (GTK_COMBO_BOX_TEXT(pStream->m_rotate), 3, "270 Degrees CW");

            float currentRotate;
            PXL_RETURN_CODE rc = gCamera->getValue(FEATURE_ROTATE, &currentRotate);
            if (API_SUCCESS(rc))
            {
                pStream->m_rotateValue = (int)currentRotate;
                gtk_combo_box_set_active (GTK_COMBO_BOX(pStream->m_rotate),(int)(currentRotate/90.0f));
            }
        }
        if (gCamera->supported(FEATURE_FLIP))
        {
            // rotate dropdown
            flipSettable = true;

            gtk_combo_box_text_remove_all (GTK_COMBO_BOX_TEXT(pStream->m_flip));

            gtk_combo_box_text_insert_text (GTK_COMBO_BOX_TEXT(pStream->m_flip), FLIP_NONE, "None");
            gtk_combo_box_text_insert_text (GTK_COMBO_BOX_TEXT(pStream->m_flip), FLIP_HORIZONTAL, "Horizonatal");
            gtk_combo_box_text_insert_text (GTK_COMBO_BOX_TEXT(pStream->m_flip), FLIP_VERTICAL, "Vertical");
            gtk_combo_box_text_insert_text (GTK_COMBO_BOX_TEXT(pStream->m_flip), FLIP_BOTH, "Both");

            bool flipX;
            bool flipY;

            PXL_RETURN_CODE rc = gCamera->getFlip(&flipX, &flipY);
            if (API_SUCCESS(rc))
            {
                pStream->m_verticalFlip = flipX;
                pStream->m_horizontalFlip = flipY;

                FLIP_TYPE flip = (flipX && flipX ? FLIP_BOTH : (flipX ? FLIP_HORIZONTAL : (flipY ? FLIP_VERTICAL : FLIP_NONE)));
                gtk_combo_box_set_active (GTK_COMBO_BOX(pStream->m_flip),flip);
            }
        }
    }

    gtk_widget_set_sensitive (pStream->m_rotate, rotateSettable);
    gtk_widget_set_sensitive (pStream->m_flip, flipSettable);

    return false;  //  Only run once....
}

/* ---------------------------------------------------------------------------
 * --   Control functions from the Glade project
 * ---------------------------------------------------------------------------
 */

extern "C" void NewRoiSelected
    (GtkWidget* widget, GdkEventExpose* event, gpointer userdata )
{
    if (! gCamera || ! gCamera) return;
    if (gStreamTab->m_numRefreshRequestsOutstanding) return;

    PxLAutoLock lock(&gCameraLock);

    int roiIndex = gtk_combo_box_get_active (GTK_COMBO_BOX(gStreamTab->m_roiCombo));
    // This routine may have been triggered by the user changing the custom ROI value, in
    // which case it is gone.  That instance is handled completely by the edit field handler
    if (roiIndex < 0 || roiIndex >= (int)gStreamTab->m_supportedRois.size()) return;

    PXL_ROI newRoi = gStreamTab->m_supportedRois[roiIndex];

    // Try to keep the new ROI centered on the old ROI.  We need to read the ROI limits
    // to determine this.
    PXL_RETURN_CODE rc;

    int oldCenterX = gStreamTab->m_roi.m_offsetX + (gStreamTab->m_roi.m_width/2);
    int oldCenterY = gStreamTab->m_roi.m_offsetY + (gStreamTab->m_roi.m_height/2);
    int newOffsetX = oldCenterX - newRoi.m_width/2;
    int newOffsetY = oldCenterY - newRoi.m_height/2;

    // However, we may need to move our center if the new offset does not fit
    if (newOffsetX < 0) newOffsetX = 0;
    if (newOffsetY < 0) newOffsetY = 0;
    if (newOffsetX + newRoi.m_width > gStreamTab->m_maxRoi.m_width) newOffsetX = gStreamTab->m_maxRoi.m_width - newRoi.m_width;
    if (newOffsetY + newRoi.m_height > gStreamTab->m_maxRoi.m_height) newOffsetY = gStreamTab->m_maxRoi.m_height - newRoi.m_height;

    newRoi.m_offsetX = newOffsetX;
    newRoi.m_offsetY = newOffsetY;

    rc = gCamera->setRoiValue(FrameRoi, newRoi);
    if (API_SUCCESS(rc))
    {
        if (rc == ApiSuccessParametersChanged)
        {
            // The camera had to adjust the parameters to make them work, read it back to get the correct values.
            rc = gCamera->getRoiValue(FrameRoi, &newRoi);
        }
    }
    if (API_SUCCESS(rc))
    {
        gStreamTab->m_roi = newRoi;

        // Update our controls to indicate we are using the new ROI
        char cValue[40];

        sprintf (cValue, "%d",newRoi.m_offsetX);
        gtk_entry_set_text (GTK_ENTRY (gStreamTab->m_offsetX), cValue);
        sprintf (cValue, "%d",newRoi.m_offsetY);
        gtk_entry_set_text (GTK_ENTRY (gStreamTab->m_offsetY), cValue);
        sprintf (cValue, "%d",newRoi.m_width);
        gtk_entry_set_text (GTK_ENTRY (gStreamTab->m_width), cValue);
        sprintf (cValue, "%d",newRoi.m_height);
        gtk_entry_set_text (GTK_ENTRY (gStreamTab->m_height), cValue);

        // If we are no longer using the 'custom' ROI at the end of our dropdown, remove it.
        if (gStreamTab->m_usingNonstandardRoi && roiIndex != gStreamTab->m_numRoisSupported-1)
        {
            gtk_combo_box_text_remove (GTK_COMBO_BOX_TEXT(gStreamTab->m_roiCombo),
                                       --gStreamTab->m_numRoisSupported);
            gStreamTab->m_supportedRois.pop_back();
            gStreamTab->m_usingNonstandardRoi = false;
        }

        if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON(gVideoPreviewTab->m_autoResize)))
        {
            rc = gCamera->resizePreviewToRoi();
        }

        // Update the ROI button
        gStreamTab->loadFfovImage();
        gStreamTab->updateRoiButton();
    }

    // Update other tabs the next time they are activated
    gControlsTab->refreshRequired(false);
#if defined VIDEO_CAPTURE_SUPPORTED
    gVideoTab->refreshRequired(false);
#endif
    gLensTab->refreshRequired(false);
    gAutoRoiTab->refreshRequired(false);
}

extern "C" void OffsetXValueChanged
    (GtkWidget* widget, GdkEventExpose* event, gpointer userdata )
{
    if (! gCamera || ! gStreamTab) return;
    if (gStreamTab->m_numRefreshRequestsOutstanding) return;

    PxLAutoLock lock(&gCameraLock);

    int newOffsetX = atoi (gtk_entry_get_text (GTK_ENTRY (gStreamTab->m_offsetX)));
    PXL_RETURN_CODE rc;

    PXL_ROI newRoi = gStreamTab->m_roi;

    if (newOffsetX + newRoi.m_width > gStreamTab->m_maxRoi.m_width) newOffsetX = gStreamTab->m_maxRoi.m_width - newRoi.m_width;
    newRoi.m_offsetX = newOffsetX;

    rc = gCamera->setRoiValue(FrameRoi, newRoi);
    if (API_SUCCESS (rc))
    {
        // Read it back again, just in case the camera did some 'tuning' of the values
        rc = gCamera->getRoiValue(FrameRoi, &newRoi);

        if (API_SUCCESS (rc))
        {
            gStreamTab->m_roi = newRoi;
        }

        // Update the ROI button
        gStreamTab->loadFfovImage();
        gStreamTab->updateRoiButton();
    }

    // Reassert the current value even if we did not succeed in setting it
    char cValue[40];
    sprintf (cValue, "%d",gStreamTab->m_roi.m_offsetX);
    gtk_entry_set_text (GTK_ENTRY (gStreamTab->m_offsetX), cValue);
}

extern "C" void OffsetYValueChanged
    (GtkWidget* widget, GdkEventExpose* event, gpointer userdata )
{
    if (! gCamera || ! gStreamTab) return;
    if (gStreamTab->m_numRefreshRequestsOutstanding) return;

    PxLAutoLock lock(&gCameraLock);

    int newOffsetY = atoi (gtk_entry_get_text (GTK_ENTRY (gStreamTab->m_offsetY)));
    PXL_RETURN_CODE rc;

    PXL_ROI newRoi = gStreamTab->m_roi;

    if (newOffsetY + newRoi.m_height > gStreamTab->m_maxRoi.m_height) newOffsetY = gStreamTab->m_maxRoi.m_height - newRoi.m_height;
    newRoi.m_offsetY = newOffsetY;

    rc = gCamera->setRoiValue(FrameRoi, newRoi);
    if (API_SUCCESS (rc))
    {
        // Read it back again, just in case the camera did some 'tuning' of the values
        rc = gCamera->getRoiValue(FrameRoi, &newRoi);

        if (API_SUCCESS (rc))
        {
            gStreamTab->m_roi = newRoi;
        }

        // Update the ROI button
        gStreamTab->loadFfovImage();
        gStreamTab->updateRoiButton();
    }

    // Reassert the current value even if we did not succeed in setting it
    char cValue[40];
    sprintf (cValue, "%d",gStreamTab->m_roi.m_offsetY);
    gtk_entry_set_text (GTK_ENTRY (gStreamTab->m_offsetY), cValue);
}

extern "C" void WidthValueChanged
    (GtkWidget* widget, GdkEventExpose* event, gpointer userdata )
{
    if (! gCamera || ! gStreamTab) return;
    if (gStreamTab->m_numRefreshRequestsOutstanding) return;

    PxLAutoLock lock(&gCameraLock);

    char cValue[40];

    // NOTE, this procedure will adjust the offsetX value, if doing so will accommodate the user
    // suppled width
    int newWidth = atoi (gtk_entry_get_text (GTK_ENTRY (gStreamTab->m_width)));
    int newOffsetX = gStreamTab->m_roi.m_offsetX;

    PXL_RETURN_CODE rc;

    PXL_ROI newRoi = gStreamTab->m_roi;

    if (newWidth + newRoi.m_offsetX > gStreamTab->m_maxRoi.m_width)
    {
        newOffsetX = gStreamTab->m_maxRoi.m_width - newWidth;
    }
    if (newOffsetX >= 0)
    {
        // Looks like we found a width and offsetX that should work
        newRoi.m_width = newWidth;
        newRoi.m_offsetX = newOffsetX;

        rc = gCamera->setRoiValue(FrameRoi, newRoi);
        if (API_SUCCESS (rc))
        {
            // Read it back again, just in case the camera did some 'tuning' of the values
            rc = gCamera->getRoiValue(FrameRoi, &newRoi);
            if (API_SUCCESS (rc))
            {
                gStreamTab->m_roi = newRoi;

                // Update the controls with the new ROI width (and offsetx, as it may have changed.  Note that
                // the actual edit field is done later, even on failure.
                sprintf (cValue, "%d",gStreamTab->m_roi.m_offsetX);
                gtk_entry_set_text (GTK_ENTRY (gStreamTab->m_offsetX), cValue);
                if (gStreamTab->m_usingNonstandardRoi)
                {
                    gtk_combo_box_text_remove (GTK_COMBO_BOX_TEXT(gStreamTab->m_roiCombo),
                                               --gStreamTab->m_numRoisSupported);
                    gStreamTab->m_supportedRois.pop_back();
                    gStreamTab->m_usingNonstandardRoi = false;
                }

                int newSelectionIndex;
                std::vector<PXL_ROI>::iterator it;
                it = find (gStreamTab->m_supportedRois.begin(), gStreamTab->m_supportedRois.end(), newRoi);
                if (it == gStreamTab->m_supportedRois.end())
                {
                    // This is a new 'non standard' ROI.  Add it to the end
                    newSelectionIndex = gStreamTab->m_numRoisSupported;
                    sprintf (cValue, "%d x %d", newRoi.m_width, newRoi.m_height);
                    gtk_combo_box_text_insert_text (GTK_COMBO_BOX_TEXT(gStreamTab->m_roiCombo),
                                                    gStreamTab->m_numRoisSupported++,
                                                    cValue);
                    gStreamTab->m_supportedRois.push_back(newRoi);
                    gStreamTab->m_usingNonstandardRoi = true;
                } else {
                    // the user 'created' a standard ROI
                    newSelectionIndex = it - gStreamTab->m_supportedRois.begin();
                }
                gtk_combo_box_set_active (GTK_COMBO_BOX(gStreamTab->m_roiCombo),newSelectionIndex);

                // Update other tabs the next time they are activated
                gControlsTab->refreshRequired(false);
#if defined VIDEO_CAPTURE_SUPPORTED
                gVideoTab->refreshRequired(false);
#endif
                gLensTab->refreshRequired(false);
                gAutoRoiTab->refreshRequired(false);
            }

            // Reset the preview window size if necessary
            if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON(gVideoPreviewTab->m_autoResize)))
            {
                rc = gCamera->resizePreviewToRoi();
            }

            // Update the ROI button
            gStreamTab->loadFfovImage();
            gStreamTab->updateRoiButton();
        }
    }

    // Reassert the current value even if we did not succeed in setting it
    sprintf (cValue, "%d",gStreamTab->m_roi.m_width);
    gtk_entry_set_text (GTK_ENTRY (gStreamTab->m_width), cValue);
}

extern "C" void HeightValueChanged
    (GtkWidget* widget, GdkEventExpose* event, gpointer userdata )
{
    if (! gCamera || ! gStreamTab) return;
    if (gStreamTab->m_numRefreshRequestsOutstanding) return;

    PxLAutoLock lock(&gCameraLock);

    char cValue[40];

    // NOTE, this procedure will adjust the offsetY value, if doing so will accommodate the user
    // suppled height
    int newHeight = atoi (gtk_entry_get_text (GTK_ENTRY (gStreamTab->m_height)));
    int newOffsetY = gStreamTab->m_roi.m_offsetY;

    PXL_RETURN_CODE rc;
    PXL_ROI newRoi = gStreamTab->m_roi;

    if (newHeight + newRoi.m_offsetY > gStreamTab->m_maxRoi.m_height)
    {
        newOffsetY = gStreamTab->m_maxRoi.m_height - newHeight;
    }
    if (newOffsetY >= 0)
    {
        // Looks like we found a width and offsetX that should work
        newRoi.m_height = newHeight;
        newRoi.m_offsetY = newOffsetY;

        rc = gCamera->setRoiValue(FrameRoi, newRoi);
        if (API_SUCCESS (rc))
        {
            // Read it back again, just in case the camera did some 'tuning' of the values
            rc = gCamera->getRoiValue(FrameRoi, &newRoi);

            if (API_SUCCESS (rc))
            {
                gStreamTab->m_roi = newRoi;

                // Update the controls with the new ROI height (and offsety, as it may have changed.  Note that
                // the actual edit field is done later, even on failure.
                sprintf (cValue, "%d",gStreamTab->m_roi.m_offsetY);
                gtk_entry_set_text (GTK_ENTRY (gStreamTab->m_offsetY), cValue);
                if (gStreamTab->m_usingNonstandardRoi)
                {
                    gtk_combo_box_text_remove (GTK_COMBO_BOX_TEXT(gStreamTab->m_roiCombo),
                                               --gStreamTab->m_numRoisSupported);
                    gStreamTab->m_supportedRois.pop_back();
                    gStreamTab->m_usingNonstandardRoi = false;
                }
                int newSelectionIndex;
                std::vector<PXL_ROI>::iterator it;
                it = find (gStreamTab->m_supportedRois.begin(), gStreamTab->m_supportedRois.end(), newRoi);
                if (it == gStreamTab->m_supportedRois.end())
                {
                    // This is a new 'non standard' ROI.  Add it to the end
                    newSelectionIndex = gStreamTab->m_numRoisSupported;
                    sprintf (cValue, "%d x %d", newRoi.m_width, newRoi.m_height);
                    gtk_combo_box_text_insert_text (GTK_COMBO_BOX_TEXT(gStreamTab->m_roiCombo),
                                                    gStreamTab->m_numRoisSupported++,
                                                    cValue);
                    gStreamTab->m_supportedRois.push_back(newRoi);
                    gStreamTab->m_usingNonstandardRoi = true;
                } else {
                    // the user 'created' a standard ROI
                    newSelectionIndex = it - gStreamTab->m_supportedRois.begin();
                }
                gtk_combo_box_set_active (GTK_COMBO_BOX(gStreamTab->m_roiCombo),newSelectionIndex);

                // Update other tab the next time they are activated
                gControlsTab->refreshRequired(false);
#if defined VIDEO_CAPTURE_SUPPORTED
                gVideoTab->refreshRequired(false);
#endif 
                gLensTab->refreshRequired(false);
                gAutoRoiTab->refreshRequired(false);
            }
            // Reset the preview window size if necessary
            if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON(gVideoPreviewTab->m_autoResize)))
            {
                rc = gCamera->resizePreviewToRoi();
            }

            // Update the ROI button
            gStreamTab->loadFfovImage();
            gStreamTab->updateRoiButton();
        }
    }

    // Reassert the current value even if we did not succeed in setting it
    sprintf (cValue, "%d",gStreamTab->m_roi.m_height);
    gtk_entry_set_text (GTK_ENTRY (gStreamTab->m_height), cValue);
}

extern "C" void CenterButtonPressed
    (GtkWidget* widget, GdkEventExpose* event, gpointer userdata )
{
    if (! gCamera || ! gCameraSelectTab || !gControlsTab) return;
    if (gStreamTab->m_numRefreshRequestsOutstanding) return;

    PxLAutoLock lock(&gCameraLock);

    PXL_RETURN_CODE rc;

    PXL_ROI newRoi = gStreamTab->m_roi;

    int newOffsetX = (gStreamTab->m_maxRoi.m_width - newRoi.m_width) / 2;
    int newOffsetY = (gStreamTab->m_maxRoi.m_height - newRoi.m_height) / 2;

    newRoi.m_offsetX = newOffsetX;
    newRoi.m_offsetY = newOffsetY;

    rc = gCamera->setRoiValue(FrameRoi, newRoi);
    if (API_SUCCESS (rc))
    {
        // Read it back again, just in case the camera did some 'tuning' of the values
        gCamera->getRoiValue(FrameRoi, &newRoi);

        gStreamTab->m_roi = newRoi;

        // Update the controls
        char cValue[40];
        sprintf (cValue, "%d",gStreamTab->m_roi.m_offsetX);
        gtk_entry_set_text (GTK_ENTRY (gStreamTab->m_offsetX), cValue);
        sprintf (cValue, "%d",gStreamTab->m_roi.m_offsetY);
        gtk_entry_set_text (GTK_ENTRY (gStreamTab->m_offsetY), cValue);

        // Update the ROI button
        gStreamTab->loadFfovImage();
        gStreamTab->updateRoiButton();
    }
}

extern "C" bool RoiButtonPress
    (GtkWidget* widget, GdkEventButton *event )
{
    if (! gCamera || ! gStreamTab) return true;
    if (gStreamTab->m_numRefreshRequestsOutstanding) return true;

    // Pressing the button will 'start' an operation, and releasing it will end one
    if (event->type == GDK_BUTTON_PRESS)
    {
        gStreamTab->startRoiButtonOperation(widget, event->x, event->y, event->x_root, event->y_root);
    } else {
        gStreamTab->finishRoiButtonOperation(widget);
    }

    return true;
}

extern "C" bool RoiButtonMotion
    (GtkWidget* widget, GdkEventButton *event )
{
    if (! gCamera || ! gStreamTab) return true;
    if (gStreamTab->m_numRefreshRequestsOutstanding) return true;

    //
    // Step 1
    //      If we are not performing an operation, than all we need to
    //      do, is set the cursor type
    if (gStreamTab->m_currentOp == OP_NONE)
    {
        gdk_window_set_cursor (gtk_widget_get_window (widget),
                               gStreamTab->getCursorForOperation ( gStreamTab->determineOperationFromMouse (event->x, event->y)));
        return true;
    }

    //
    // Step 2
    //      We are performing some sort of operation IE, the user is dragging the mouse.  We need to redraw
    //      the ROI button as the user does the drag
    int deltaX = (int) (event->x_root -  gStreamTab->m_mouseXOnOpStart);
    int deltaY = (int) (event->y_root -  gStreamTab->m_mouseYOnOpStart);

    // define some local variables just to make the code more compact and readable
    int *width = &gStreamTab->m_roiButtonRoi.m_width;
    int *height = &gStreamTab->m_roiButtonRoi.m_height;
    int *x = &gStreamTab->m_roiButtonRoi.m_offsetX;
    int *y = &gStreamTab->m_roiButtonRoi.m_offsetY;
    int xMin = gStreamTab->m_roiButtonLimits.xMin;
    int xMax = gStreamTab->m_roiButtonLimits.xMax;
    int yMin = gStreamTab->m_roiButtonLimits.yMin;
    int yMax = gStreamTab->m_roiButtonLimits.yMax;
    int widthStart =  gStreamTab->m_roiButtonOnOpStart.m_width;
    int heightStart = gStreamTab->m_roiButtonOnOpStart.m_height;
    int xStart =  gStreamTab->m_roiButtonOnOpStart.m_offsetX;
    int yStart = gStreamTab->m_roiButtonOnOpStart.m_offsetY;

    switch (gStreamTab->m_currentOp)
    {
    case OP_MOVE:
        *x = min (max (xMin, xStart + deltaX), xMax - *width);
        *y = min (max (yMin, yStart + deltaY), yMax - *height);
        gtk_widget_set_margin_left (widget, *x);
        gtk_widget_set_margin_top (widget, *y);
        break;
    case OP_RESIZE_TOP_LEFT:
        *x = min (max (xMin, xStart + deltaX), xStart + widthStart - ROI_MIN_WIDTH);
        *y = min (max (yMin, yStart + deltaY), yStart + heightStart - ROI_MIN_HEIGHT);
        gtk_widget_set_margin_left (widget, *x);
        gtk_widget_set_margin_top (widget, *y);
        *width = min (max (ROI_MIN_WIDTH, widthStart - deltaX), xStart + widthStart - xMin);
        *height = min (max (ROI_MIN_HEIGHT, heightStart - deltaY), yStart + heightStart - yMin);
        gStreamTab->setRoiButtonSize();
        break;
    case OP_RESIZE_TOP_RIGHT:
        *y = min (max (yMin, yStart + deltaY), yStart + heightStart - ROI_MIN_HEIGHT);
        gtk_widget_set_margin_top (widget, *y);
        *width = min (max (ROI_MIN_WIDTH, widthStart + deltaX), xMax - xStart);
        *height = min (max (ROI_MIN_HEIGHT, heightStart - deltaY), yStart + heightStart - yMin);
        gStreamTab->setRoiButtonSize();
        break;
    case OP_RESIZE_BOTTOM_RIGHT:
        *width = min (max (ROI_MIN_WIDTH, widthStart + deltaX), xMax - xStart);
        *height = min (max (ROI_MIN_HEIGHT, heightStart + deltaY), yMax - yStart);
        gStreamTab->setRoiButtonSize();
        break;
    case OP_RESIZE_BOTTOM_LEFT:
        *x = min (max (xMin, xStart + deltaX), xStart + widthStart - ROI_MIN_WIDTH);
        gtk_widget_set_margin_left (widget, *x);
        *width = min (max (ROI_MIN_WIDTH, widthStart - deltaX), xStart + widthStart - xMin);
        *height = min (max (ROI_MIN_HEIGHT, heightStart + deltaY), yMax - yStart);
        gStreamTab->setRoiButtonSize();
        break;
    case OP_RESIZE_TOP:
        *y = min (max (yMin, yStart + deltaY), yStart + heightStart - ROI_MIN_HEIGHT);
        gtk_widget_set_margin_top (widget, *y);
        *height = min (max (ROI_MIN_HEIGHT, heightStart - deltaY), yStart + heightStart - yMin);
        gStreamTab->setRoiButtonSize();
        break;
    case OP_RESIZE_RIGHT:
        *width = min ( max (ROI_MIN_WIDTH, widthStart + deltaX), xMax - xStart);
        gStreamTab->setRoiButtonSize();
        break;
    case OP_RESIZE_BOTTOM:
        *height = min ( max (ROI_MIN_HEIGHT, heightStart + deltaY), yMax - yStart);
        gStreamTab->setRoiButtonSize();
        break;
    case OP_RESIZE_LEFT:
        *x = min (max (xMin, xStart + deltaX), xStart + widthStart - ROI_MIN_WIDTH);
        gtk_widget_set_margin_left (widget, *x);
        *width = min (max (ROI_MIN_WIDTH, widthStart - deltaX), xStart + widthStart - xMin);
        gStreamTab->setRoiButtonSize();
        break;
    default:
    case OP_NONE:
        // Just for completeness
        break;
    }

    return true;
}

extern "C" bool RoiButtonEnter
    (GtkWidget* widget, GdkEventButton *event )
{
    if (! gCamera || ! gStreamTab) return true;
    if (gStreamTab->m_numRefreshRequestsOutstanding) return true;

    // TEMP PEC +++
    //      NOTE.  I don't think I even need this event.
    // ---
    // If we are performing an operation (the user has the button pressed), then don't take any
    // action on leave/enter -- wait for the user to complete.
    if (gStreamTab->m_currentOp != OP_NONE) return TRUE;

    // The user is not performing an operation.
    //gdk_window_set_cursor (gtk_widget_get_window (widget),
    //                       gStreamTab->getCursorForOperation (gStreamTab->determineOperationFromMouse (event->x, event->y)));

    return true;
}

extern "C" void NewPixelFormatSelected
    (GtkWidget* widget, GdkEventExpose* event, gpointer userdata )
{
    if (! gCamera || ! gStreamTab) return;
    if (gStreamTab->m_numRefreshRequestsOutstanding) return;

    PxLAutoLock lock(&gCameraLock);

    gint pixelFormatIndex = gtk_combo_box_get_active (GTK_COMBO_BOX(gStreamTab->m_pixelFormat));

    float newPixelFormat = PxLPixelFormat::toApi(gStreamTab->m_supportedPixelFormats[pixelFormatIndex]);
    gCamera->setValue(FEATURE_PIXEL_FORMAT, newPixelFormat);

    // Reassert the pixel format interpretations and alpha, as they may have changed.
    gdk_threads_add_idle ((GSourceFunc)PixelFormatInterpretationActivate, gStreamTab);
    gdk_threads_add_idle ((GSourceFunc)AlphaActivate, gStreamTab);
    // Also, compression value may have changed
    gdk_threads_add_idle ((GSourceFunc)CompressionActivate, gStreamTab);

    // Update other tabs the next time they are activated
    gControlsTab->refreshRequired(false);
#if defined VIDEO_CAPTURE_SUPPORTED
    gVideoTab->refreshRequired(false);
#endif
    gFilterTab->refreshRequired(false);
}

extern "C" void CompressionToggled
    (GtkWidget* widget, GdkEventExpose* event, gpointer userdata )
{
    if (! gCamera || ! gStreamTab) return;
    if (gStreamTab->m_numRefreshRequestsOutstanding) return;
    // Don't bother to check to see if it's supported here -- the control will only be activated if the
    // camera supporte it
    //if (!gCamera->supported(FEATURE_COPRESSION)) return;

    bool enable = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON(gStreamTab->m_compression));

    PxLAutoLock lock(&gCameraLock);

    PXL_RETURN_CODE rc;
    float compressionParams[2];

    rc = gCamera->getValue (FEATURE_PIXEL_FORMAT, compressionParams);

    if (!API_SUCCESS(rc)) return;

    // Currently, the only compression format we know, is Pixelink10
    compressionParams[1] = enable ? FEATURE_COMPRESSION_STRATEGY_PIXELINK10 :
                                    FEATURE_COMPRESSION_STRATEGY_NONE;

    gCamera->set2ndValue (FEATURE_COMPRESSION, compressionParams[0], compressionParams[1]);
}

extern "C" void NewPixelFormatInterpretationSelected
    (GtkWidget* widget, GdkEventExpose* event, gpointer userdata )
{
    if (! gCamera || ! gStreamTab) return;
    if (gStreamTab->m_numRefreshRequestsOutstanding) return;

    PxLAutoLock lock(&gCameraLock);

    COEM_PIXEL_FORMAT_INTERPRETATIONS interpretation = (COEM_PIXEL_FORMAT_INTERPRETATIONS)gtk_combo_box_get_active (GTK_COMBO_BOX(gStreamTab->m_pixelFormatInterpretation));

    if (interpretation >= 0 && interpretation < NUM_COEM_PIXEL_FORMAT_INTERPRETATIONS)
    {
        gCamera->setValue(FEATURE_POLAR_HSV_INTERPRETATION, (float)interpretation);
    }
}

extern "C" void NewPixelAddressModeSelected
    (GtkWidget* widget, GdkEventExpose* event, gpointer userdata )
{
    if (! gCamera || ! gStreamTab) return;
    if (gStreamTab->m_numRefreshRequestsOutstanding) return;

    PxLAutoLock lock(&gCameraLock);

    int modeIndex = gtk_combo_box_get_active (GTK_COMBO_BOX(gStreamTab->m_pixelAddressingMode));
    int valueIndex = gtk_combo_box_get_active (GTK_COMBO_BOX(gStreamTab->m_pixelAddressingValue));

    // Don't bother to set the new mode, if the current value is PA_NONE, or negative (uninitialized)
    if (modeIndex < 0 || valueIndex<0) return;
    if (gStreamTab->m_supportedPixelAddressValues[valueIndex] == PA_NONE) return;

    float paValueX, paValueY;
    PxLPixelAddress::valueToApi(gStreamTab->m_supportedPixelAddressValues[valueIndex], &paValueX, &paValueY);
    gCamera->setPixelAddressValues(
            PxLPixelAddress::modeToApi(gStreamTab->m_supportedPixelAddressModes[modeIndex]), paValueX, paValueY);

    // Update the ROI button
    gStreamTab->loadFfovImage();
    gStreamTab->updateRoiButton();

    // Update other tab the next time they are activated
    gControlsTab->refreshRequired(false);
#if defined VIDEO_CAPTURE_SUPPORTED
    gVideoTab->refreshRequired(false);
#endif
}

extern "C" void AlphaValueChanged
    (GtkWidget* widget, GdkEventExpose* event, gpointer userdata )
{
    if (! gCamera || ! gStreamTab) return;
    if (gStreamTab->m_numRefreshRequestsOutstanding) return;

    PxLAutoLock lock(&gCameraLock);

    char cValue[40];

    int currentPixelFormat = -1;
    ULONG currentAlpha = 0;
    int newAlpha = atoi (gtk_entry_get_text (GTK_ENTRY (gStreamTab->m_alpha)));

    PXL_RETURN_CODE rc;

    rc = gCamera->getAlpha (&currentPixelFormat, &currentAlpha);

    if (API_SUCCESS(rc) && currentPixelFormat >= 0)
    {
        rc = gCamera->setAlpha (currentPixelFormat, (ULONG)newAlpha);
    }

    // Get the alpha again, regardless if we succeeded or not
    rc = gCamera->getAlpha (&currentPixelFormat, &currentAlpha);
    sprintf (cValue, "%d",currentAlpha);
    gtk_entry_set_text (GTK_ENTRY (gStreamTab->m_alpha), cValue);
}

extern "C" void NewPixelAddressValueSelected
    (GtkWidget* widget, GdkEventExpose* event, gpointer userdata )
{
    if (! gCamera || ! gStreamTab) return;
    if (gStreamTab->m_numRefreshRequestsOutstanding) return;

    PxLAutoLock lock(&gCameraLock);

    int modeIndex = gtk_combo_box_get_active (GTK_COMBO_BOX(gStreamTab->m_pixelAddressingMode));
    int valueIndex = gtk_combo_box_get_active (GTK_COMBO_BOX(gStreamTab->m_pixelAddressingValue));

    // Don't bother to set the new mode, if the current value negative (uninitialized)
    if (modeIndex < 0 || valueIndex<0) return;

    float paValueX, paValueY;
    PxLPixelAddress::valueToApi(gStreamTab->m_supportedPixelAddressValues[valueIndex], &paValueX, &paValueY);
    gCamera->setPixelAddressValues(
            PxLPixelAddress::modeToApi(gStreamTab->m_supportedPixelAddressModes[modeIndex]), paValueX, paValueY);

    // Reset the preview window size if necessary
    if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON(gVideoPreviewTab->m_autoResize)))
    {
        gCamera->resizePreviewToRoi();
    }

    // Update the ROI button
    gStreamTab->loadFfovImage();
    gStreamTab->updateRoiButton();

    // Update other tabs the next time they are activated
    gControlsTab->refreshRequired(false);
#if defined VIDEO_CAPTURE_SUPPORTED
    gVideoTab->refreshRequired(false);
#endif
}

extern "C" void NewRotateValueSelected
    (GtkWidget* widget, GdkEventExpose* event, gpointer userdata )
{
    if (! gCamera || ! gStreamTab) return;
    if (gStreamTab->m_numRefreshRequestsOutstanding) return;

    PxLAutoLock lock(&gCameraLock);

    int rotateIndex = gtk_combo_box_get_active (GTK_COMBO_BOX(gStreamTab->m_rotate));

    float rotateValue = (float)rotateIndex*90;
    PXL_RETURN_CODE rc = gCamera->setValue(FEATURE_ROTATE, rotateValue);
    if (! API_SUCCESS(rc))
    {
        // It didn't work.  Read the current value and show the correct one in the dropdown.
        gCamera->getValue(FEATURE_ROTATE, &rotateValue);
        gtk_combo_box_set_active (GTK_COMBO_BOX(gStreamTab->m_rotate),(int)(rotateValue/90.0f));
    }
    gStreamTab->m_rotateValue = (int)rotateValue;

    // Update the ROI information
    gdk_threads_add_idle ((GSourceFunc)RoiActivate, gStreamTab);
    gStreamTab->m_numRefreshRequestsOutstanding++; // We don't want any controls to trigger a change while we are updating them
    gdk_threads_add_idle ((GSourceFunc)RefreshComplete, gStreamTab);

    // Let other tabs know an update is required
    gLensTab->refreshRequired(false);
    gAutoRoiTab->refreshRequired(false);
}

extern "C" void NewFlipValueSelected
    (GtkWidget* widget, GdkEventExpose* event, gpointer userdata )
{
    if (! gCamera || ! gStreamTab) return;
    if (gStreamTab->m_numRefreshRequestsOutstanding) return;

    PxLAutoLock lock(&gCameraLock);

    int flipIndex = gtk_combo_box_get_active (GTK_COMBO_BOX(gStreamTab->m_flip));
    bool flipX = flipIndex == FLIP_HORIZONTAL || flipIndex == FLIP_BOTH;
    bool flipY = flipIndex == FLIP_VERTICAL || flipIndex == FLIP_BOTH;

    PXL_RETURN_CODE rc = gCamera->setFlip(flipX, flipY);
    if (! API_SUCCESS(rc))
    {
        // It didn't work.  Read the current value and show the correct one in the dropdown.
        gCamera->getFlip(&flipX, &flipY);
        FLIP_TYPE flip = (flipX && flipX ? FLIP_BOTH : (flipX ? FLIP_HORIZONTAL : (flipY ? FLIP_VERTICAL : FLIP_NONE)));
        gtk_combo_box_set_active (GTK_COMBO_BOX(gStreamTab->m_flip),flip);
    }
    gStreamTab->m_verticalFlip = flipX;
    gStreamTab->m_horizontalFlip = flipY;

    // Update the ROI information
    gdk_threads_add_idle ((GSourceFunc)RoiActivate, gStreamTab);
    gStreamTab->m_numRefreshRequestsOutstanding++; // We don't want any controls to trigger a change while we are updating them
    gdk_threads_add_idle ((GSourceFunc)RefreshComplete, gStreamTab);

    // Let other tabs know an update is required
    gLensTab->refreshRequired(false);
    gAutoRoiTab->refreshRequired(false);
}



