
/***************************************************************************
 *
 *     File: lens.cpp
 *
 *     Description:
 *        Controls for the 'Lens' tab  in CaptureOEM.
 */

#include <algorithm>
#include "lens.h"
#include "cameraSelect.h"
#include "camera.h"
#include "captureOEM.h"
#include "preview.h"
#include "stream.h"
#include "onetime.h"

using namespace std;

extern PxLLens         *gLensTab;
extern PxLCameraSelect *gCameraSelectTab;
extern PxLPreview      *gVideoPreviewTab;
extern PxLOnetime      *gOnetimeDialog;
//
// Local prototypes.
//    UI updates can only be done from a gtk thread -- these routines are gtk 'idle' threads
//    and as such are 'UI update safe'. For each 'feature', there there the following functions:
//       . {featureXXX}Deactivate - Makes the controls meaningless (including greying them out)
//       . {featreuXXX}Activate - updates the controls with values from the camera
static gboolean  RefreshComplete (gpointer pData);
static gboolean  SsroiDeactivate (gpointer pData);
static gboolean  SsroiActivate (gpointer pData);
static gboolean  SsUpdate (gpointer pData);
static gboolean  ControllerDeactivate (gpointer pData);
static gboolean  ControllerActivate (gpointer pData);
static gboolean  FocusDeactivate (gpointer pData);
static gboolean  FocusActivate (gpointer pData);
static gboolean  ZoomDeactivate (gpointer pData);
static gboolean  ZoomActivate (gpointer pData);

extern "C" void NewSsroiSelected (GtkWidget* widget, GdkEventExpose* event, gpointer userdata);

extern "C" bool SsroiButtonPress  (GtkWidget* widget, GdkEventButton *event );
extern "C" bool SsroiButtonMotion (GtkWidget* widget, GdkEventButton *event );
extern "C" bool SsroiButtonEnter  (GtkWidget* widget, GdkEventButton *event );

PXL_RETURN_CODE GetCurrentFocus();
void UpdateFocusControls();
const PxLFeaturePollFunctions focusFuncs (GetCurrentFocus, UpdateFocusControls);

static void SsFrameCallback (float sharpnessScore);

/* ---------------------------------------------------------------------------
 * --   Static data used to construct our dropdowns - Private
 * ---------------------------------------------------------------------------
 */

static const char  SsroiLabel[] = "Current\n SS-ROI";


/* ---------------------------------------------------------------------------
 * --   Member functions - Public
 * ---------------------------------------------------------------------------
 */
PxLLens::PxLLens (GtkBuilder *builder)
: m_roiBuf(NULL)
, m_roiAspectRatio((float)ROI_AREA_WIDTH / (float)ROI_AREA_HEIGHT)
, m_currentOp(OP_NONE)
, m_maxSs (0)
{
    //
    // Step 1
    //      Find all of the glade controls

    // Roi
    m_ssroiCombo = GTK_WIDGET( gtk_builder_get_object( builder, "Ssroi_Combo" ) );
    m_offsetX = GTK_WIDGET( gtk_builder_get_object( builder, "SsroiOffsetX_Text" ) );
    m_offsetY = GTK_WIDGET( gtk_builder_get_object( builder, "SsroiOffsetY_Text" ) );
    m_width = GTK_WIDGET( gtk_builder_get_object( builder, "SsroiWidth_Text" ) );
    m_height = GTK_WIDGET( gtk_builder_get_object( builder, "SsroiHeight_Text" ) );
    m_center = GTK_WIDGET( gtk_builder_get_object( builder, "SsroiCenter_Button" ) );
    m_roiImage = GTK_WIDGET( gtk_builder_get_object( builder, "Ssroi_Image" ) );
    m_ssroiButton = GTK_WIDGET( gtk_builder_get_object( builder, "Ssroi_Button" ) );
    m_sharpnessScore = GTK_WIDGET( gtk_builder_get_object( builder, "SharpnessScore_Text" ) );

    // connect elements from the style sheet, to our builder
    gtk_widget_set_name (m_ssroiButton, "Ssroi_Button_red");

    m_controllerCombo = GTK_WIDGET( gtk_builder_get_object( builder, "Controller_Combo" ) );

    m_focusSlider = new PxLSlider (
            GTK_WIDGET( gtk_builder_get_object( builder, "FocusMin_Label" ) ),
            GTK_WIDGET( gtk_builder_get_object( builder, "FocusMax_Label" ) ),
            GTK_WIDGET( gtk_builder_get_object( builder, "Focus_Scale" ) ),
            GTK_WIDGET( gtk_builder_get_object( builder, "Focus_Text" ) ));
    m_focusAutoMin = GTK_WIDGET( gtk_builder_get_object( builder, "FocusAutoMin_Label" ) );
    m_focusSetAutoMin = GTK_WIDGET( gtk_builder_get_object( builder, "FocusAutoMin_Button" ) );
    m_focusAutoMax = GTK_WIDGET( gtk_builder_get_object( builder, "FocusAutoMax_Label" ) );
    m_focusSetAutoMax = GTK_WIDGET( gtk_builder_get_object( builder, "FocusAutoMax_Button" ) );
    m_focusOneTime  = GTK_WIDGET( gtk_builder_get_object( builder, "FocusOneTime_Button" ) );

    m_zoomSlider = new PxLSlider (
            GTK_WIDGET( gtk_builder_get_object( builder, "ZoomMin_Label" ) ),
            GTK_WIDGET( gtk_builder_get_object( builder, "ZoomMax_Label" ) ),
            GTK_WIDGET( gtk_builder_get_object( builder, "Zoom_Scale" ) ),
            GTK_WIDGET( gtk_builder_get_object( builder, "Zoom_Text" ) ));
    m_zoomAssertMin = GTK_WIDGET( gtk_builder_get_object( builder, "ZoomMin_Button" ) );
    m_zoomAssertMax = GTK_WIDGET( gtk_builder_get_object( builder, "ZoomMax_Button" ) );

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
    g_signal_connect (m_ssroiButton, "button_press_event",   G_CALLBACK (SsroiButtonPress), NULL);
    g_signal_connect (m_ssroiButton, "button_release_event", G_CALLBACK (SsroiButtonPress), NULL);
    g_signal_connect (m_ssroiButton, "motion_notify_event",  G_CALLBACK (SsroiButtonMotion), NULL);
    g_signal_connect (m_ssroiButton, "enter_notify_event",   G_CALLBACK (SsroiButtonEnter), NULL);
    g_signal_connect (m_ssroiButton, "leave_notify_event",   G_CALLBACK (SsroiButtonEnter), NULL);

    gtk_widget_set_events (m_ssroiButton,
                           GDK_ENTER_NOTIFY_MASK | GDK_LEAVE_NOTIFY_MASK |
                           GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK |
                           GDK_POINTER_MOTION_MASK | GDK_POINTER_MOTION_HINT_MASK);
}

PxLLens::~PxLLens ()
{
}

void PxLLens::refreshRequired (bool noCamera)
{
    m_roiBuf = NULL;

    if (IsActiveTab (LensTab))
    {
        if (noCamera)
        {
            // If I am the active tab, then grey out everything
            gdk_threads_add_idle ((GSourceFunc)SsroiDeactivate, this);
            gdk_threads_add_idle ((GSourceFunc)ControllerDeactivate, this);
            gdk_threads_add_idle ((GSourceFunc)FocusDeactivate, this);
            gdk_threads_add_idle ((GSourceFunc)ZoomDeactivate, this);

            if (gCamera)gCamera->setSsUpdateCallback (NULL);
        } else {
            // If I am the active tab, then refresh everything
            gdk_threads_add_idle ((GSourceFunc)SsroiActivate, this);
            gdk_threads_add_idle ((GSourceFunc)ControllerActivate, this);
            gdk_threads_add_idle ((GSourceFunc)FocusActivate, this);
            gdk_threads_add_idle ((GSourceFunc)ZoomActivate, this);

            gCamera->setSsUpdateCallback (SsFrameCallback);
        }
        gdk_threads_add_idle ((GSourceFunc)RefreshComplete, this);
        m_numRefreshRequestsOutstanding++;
    } else {
        // If we are not the active tab, only bump the m_numRefreshRequestsOutstanding if there is not
        // one outstanding already; RefreshComplete will be scheduled when the tab becomes active
        if (!m_numRefreshRequestsOutstanding)m_numRefreshRequestsOutstanding++;
    }
}

void PxLLens::activate()
{
    // I have become the active tab.

    if (gCamera)
    {
        if (m_numRefreshRequestsOutstanding)
        {
            gdk_threads_add_idle ((GSourceFunc)SsroiActivate, this);
            gdk_threads_add_idle ((GSourceFunc)ControllerActivate, this);
            gdk_threads_add_idle ((GSourceFunc)FocusActivate, this);
            gdk_threads_add_idle ((GSourceFunc)ZoomActivate, this);

        }
        gCamera->setSsUpdateCallback (SsFrameCallback);
    } else {
        gdk_threads_add_idle ((GSourceFunc)SsroiDeactivate, this);
        gdk_threads_add_idle ((GSourceFunc)ControllerDeactivate, this);
        gdk_threads_add_idle ((GSourceFunc)FocusDeactivate, this);
        gdk_threads_add_idle ((GSourceFunc)ZoomDeactivate, this);
    }

    m_numRefreshRequestsOutstanding = 1; // As a safety mechanism, tab activation should assert value, it will be set to 0 when RefreshComplete
    gdk_threads_add_idle ((GSourceFunc)RefreshComplete, this);
}

void PxLLens::deactivate()
{
    // I am no longer the active tab.
    if (gCamera) {
        gCamera->m_poller->pollRemove(focusFuncs);
        gCamera->setSsUpdateCallback (NULL);
    }
}

// indication that the app has transitioned to/from playing state.
void PxLLens::playChange (bool playing)
{
    if (IsActiveTab (LensTab)) loadRoiImage();
}

// Load the ROI background image.  streamInterrupted is true if the stream is supposed to be running, but
// has temporarily been turned off to perform some operations.
void PxLLens::loadRoiImage(bool streamInterrupted)
{
    //
    // Step 1
    //      If we don't have a camera, load the 'No Camera' bmp
    if (! gCamera)
    {
        // No camera
        if (m_roiBuf == NULL)
        {
            // Nothing loaded in the FFOV area, load a default message
            m_roiBuf = gdk_pixbuf_new_from_file_at_size ("NoCamera.bmp",
                                                          ROI_AREA_WIDTH, ROI_AREA_HEIGHT, NULL);
            gtk_image_set_from_pixbuf (GTK_IMAGE (m_roiImage), m_roiBuf);
        }
    } else {
        PXL_RETURN_CODE rc;

        // Step 2.
        //      Figure out the aspect ration of the ROI and ROI image area
        float roiAreaAspectRatio = (float)ROI_AREA_WIDTH / (float)ROI_AREA_HEIGHT;

        if (m_roiAspectRatio > roiAreaAspectRatio)
        {
            // height limited
            m_ssroiButtonLimits.xMax = ROI_AREA_WIDTH;
            m_ssroiButtonLimits.yMax = (int)((float)ROI_AREA_WIDTH / m_roiAspectRatio);
            m_ssroiButtonLimits.xMin = 0;
            m_ssroiButtonLimits.yMin = (ROI_AREA_HEIGHT - m_ssroiButtonLimits.yMax) / 2 ;
            m_ssroiButtonLimits.yMax += m_ssroiButtonLimits.yMin;
        } else {
            // width limited (or unlimited (same aspect ratio)
            m_ssroiButtonLimits.xMax = (int)((float)ROI_AREA_HEIGHT * m_roiAspectRatio);
            m_ssroiButtonLimits.yMax = ROI_AREA_HEIGHT;
            m_ssroiButtonLimits.xMin = (ROI_AREA_WIDTH - m_ssroiButtonLimits.xMax) / 2 ;
            m_ssroiButtonLimits.xMax += m_ssroiButtonLimits.xMin;
            m_ssroiButtonLimits.yMin = 0 ;
        }

        //
        // Step 3.
        //      We want to grab a FFOV image from the camera if we can.  However we can only grab
        //      an image if the user has the camera is in a non-triggered streaming state.
        if (! gCamera->triggering() &&
            (streamInterrupted ||  gCamera->streaming()))
        {

            //
            // Step 4
            //      If the stream was temporarily disabled, turn it on
            if (streamInterrupted) gCamera->startStream();

            //
            // Step 5
            //      Grab and format the image
            std::vector<U8> frameBuf (gCamera->imageSizeInBytes());
            FRAME_DESC     frameDesc;

            rc = gCamera->getNextFrame(frameBuf.size(), &frameBuf[0], &frameDesc);
            if (API_SUCCESS(rc))
            {
                // Allocate an rgb Buffer.
                std::vector<U8> rgbBuffer (frameBuf.size()*3);
                rc = gCamera->formatRgbImage(&frameBuf[0], &frameDesc, rgbBuffer.size(), &rgbBuffer[0]);
                if (API_SUCCESS(rc))
                {
                    //
                    // Step 6
                    //      Convert the rgb image into a roi buffer.  Be sure to use the same aspect ratio as the roi


                    GdkPixbuf  *tempPixBuf = gdk_pixbuf_new_from_data (
                            &rgbBuffer[0], GDK_COLORSPACE_RGB, false, 8,
                            (int)(frameDesc.Roi.fWidth / frameDesc.PixelAddressingValue.fHorizontal),
                            (int)(frameDesc.Roi.fHeight / frameDesc.PixelAddressingValue.fVertical),
                            (int)(frameDesc.Roi.fWidth / frameDesc.PixelAddressingValue.fHorizontal)*3, NULL, NULL);
                    m_roiBuf = gdk_pixbuf_scale_simple (tempPixBuf,
                            m_ssroiButtonLimits.xMax - m_ssroiButtonLimits.xMin,
                            m_ssroiButtonLimits.yMax - m_ssroiButtonLimits.yMin,
                            GDK_INTERP_BILINEAR);
                    gtk_image_set_from_pixbuf (GTK_IMAGE (m_roiImage), m_roiBuf);
                }
            }

            //
            // Step 7
            //      Restore the stream as necessary
            if (streamInterrupted) gCamera->stopStream ();
        } else {
            //
            // Step 8
            //      We can't grab an image.  Display the No Stream FFOV image (if we're not
            //      already displaying it
            if (m_roiBuf == NULL)
            {
                m_roiBuf = gdk_pixbuf_new_from_file_at_scale ("NoStream.bmp",
                        m_ssroiButtonLimits.xMax - m_ssroiButtonLimits.xMin,
                        m_ssroiButtonLimits.yMax - m_ssroiButtonLimits.yMin
                        , false, NULL);
                gtk_image_set_from_pixbuf (GTK_IMAGE (m_roiImage), m_roiBuf);
            }
        }
    }
}

// This will update the SSROI button so that it reflects the current SSROI.  Assumes m_ssroi and m_currentRoi are valid
void PxLLens::updateSsroiButton()
{
    // The button should only be visible if we have a camera
    if (gCamera)
    {
        gtk_widget_show (m_ssroiButton);

        float widthPercent =   (float)m_ssroi.m_width / (float)m_currentRoi.m_width;
        float heightPercent =  (float)m_ssroi.m_height / (float)m_currentRoi.m_height;
        float offsetXPercent = (float)m_ssroi.m_offsetX / (float)m_currentRoi.m_width;
        float offsetYPercent = (float)m_ssroi.m_offsetY / (float)m_currentRoi.m_height;

        float xLimit = (float)(m_ssroiButtonLimits.xMax - m_ssroiButtonLimits.xMin);
        float yLimit = (float)(m_ssroiButtonLimits.yMax - m_ssroiButtonLimits.yMin);

        m_ssroiButtonRoi.m_width =  (int)(xLimit * widthPercent);
        m_ssroiButtonRoi.m_height = (int)(yLimit * heightPercent);
        m_ssroiButtonRoi.m_offsetX =  (int)(xLimit * offsetXPercent) + m_ssroiButtonLimits.xMin;
        m_ssroiButtonRoi.m_offsetY =  (int)(yLimit * offsetYPercent) + m_ssroiButtonLimits.yMin;
        setSsroiButtonSize ();
        // Bugzilla.1304 -- This deprecated function is necessary because Ubuntu 14.04 on ARM uses an old library
        gtk_widget_set_margin_left (m_ssroiButton, m_ssroiButtonRoi.m_offsetX);
        gtk_widget_set_margin_top (m_ssroiButton, m_ssroiButtonRoi.m_offsetY);
    } else {
        gtk_widget_hide (m_ssroiButton);
    }
}

// Returns the operatation type for a given mouse position
ROI_BUTTON_OPS PxLLens::determineOperationFromMouse (double relativeX, double relativeY)
{
    if (relativeY <= m_EdgeSensitivity)
    {
        if (relativeX <= m_EdgeSensitivity) return OP_RESIZE_TOP_LEFT;
        if (relativeX >= m_ssroiButtonRoi.m_width-m_EdgeSensitivity) return OP_RESIZE_TOP_RIGHT;
        return OP_RESIZE_TOP;
    } else if (relativeY >= m_ssroiButtonRoi.m_height-m_EdgeSensitivity) {
        if (relativeX <= m_EdgeSensitivity) return OP_RESIZE_BOTTOM_LEFT;
        if (relativeX >= m_ssroiButtonRoi.m_width-m_EdgeSensitivity) return OP_RESIZE_BOTTOM_RIGHT;
        return OP_RESIZE_BOTTOM;
    } else if (relativeX <= m_EdgeSensitivity) {
        return OP_RESIZE_LEFT;
    } else if (relativeX >= m_ssroiButtonRoi.m_width-m_EdgeSensitivity) {
        return OP_RESIZE_RIGHT;
    } else return OP_MOVE;

}

// Mouse down on the ROI button will start an operation
void PxLLens::startSsroiButtonOperation( GtkWidget *widget, double relativeX, double relativeY, double absoluteX, double absoluteY)
{
    m_mouseXOnOpStart = absoluteX;
    m_mouseYOnOpStart = absoluteY;

    m_ssroiButtonOnOpStart = m_ssroiButtonRoi;

    m_currentOp = determineOperationFromMouse (relativeX, relativeY);

}

// Mouse up on the ROI button will finish an operation
void PxLLens::finishSsroiButtonOperation( GtkWidget *widget)
{
    bool bPreviewRestoreRequired = false;
    bool bStreamRestoreRequired = false;

    //
    // Step 1
    //      Has the operation resulted in a new SSROI?  That will only be true if the user has moved
    //      mouse sufficiently while in a mouse down state
    if (abs (m_ssroiButtonRoi.m_width   - m_ssroiButtonOnOpStart.m_width)   >= m_EdgeSensitivity ||
        abs (m_ssroiButtonRoi.m_height  - m_ssroiButtonOnOpStart.m_height)  >= m_EdgeSensitivity ||
        abs (m_ssroiButtonRoi.m_offsetX - m_ssroiButtonOnOpStart.m_offsetX) >= m_EdgeSensitivity ||
        abs (m_ssroiButtonRoi.m_offsetY - m_ssroiButtonOnOpStart.m_offsetY) >= m_EdgeSensitivity)
    {
        //
        // Step 2
        //      Figure out what SSROI user wants based on the current size and position of the SSROI window
        float xLimit = (float)(m_ssroiButtonLimits.xMax - m_ssroiButtonLimits.xMin);
        float yLimit = (float)(m_ssroiButtonLimits.yMax - m_ssroiButtonLimits.yMin);

        float widthPercent =   (float)m_ssroiButtonRoi.m_width / xLimit;
        float heightPercent =  (float)m_ssroiButtonRoi.m_height / yLimit;
        float offsetXPercent = (float)(m_ssroiButtonRoi.m_offsetX - m_ssroiButtonLimits.xMin) / xLimit;
        float offsetYPercent = (float)(m_ssroiButtonRoi.m_offsetY - m_ssroiButtonLimits.yMin) / yLimit;

        //
        //  Step 3
        //      Rounding is going to occur because of the scaling we use for the SSROI (button) representation.  However,
        //      limit those rounding errors to just the dimensions that the user is adjusting
        PXL_ROI newSsroi = m_ssroi;
        switch (m_currentOp)
        {
        case OP_MOVE:
            newSsroi.m_offsetX =  (int)((float)m_currentRoi.m_width * offsetXPercent);
            newSsroi.m_offsetY =  (int)((float)m_currentRoi.m_height * offsetYPercent);
            break;
        case OP_RESIZE_TOP_LEFT:
            newSsroi.m_width =  (int)((float)m_currentRoi.m_width * widthPercent);
            newSsroi.m_height = (int)((float)m_currentRoi.m_height * heightPercent);
            newSsroi.m_offsetX =  (int)((float)m_currentRoi.m_width * offsetXPercent);
            newSsroi.m_offsetY =  (int)((float)m_currentRoi.m_height * offsetYPercent);
            break;
        case OP_RESIZE_TOP_RIGHT:
            newSsroi.m_width =  (int)((float)m_currentRoi.m_width * widthPercent);
            newSsroi.m_height = (int)((float)m_currentRoi.m_height * heightPercent);
            newSsroi.m_offsetX =  (int)((float)m_currentRoi.m_width * offsetXPercent);
            break;
        case OP_RESIZE_BOTTOM_RIGHT:
            newSsroi.m_width =  (int)((float)m_currentRoi.m_width * widthPercent);
            newSsroi.m_height = (int)((float)m_currentRoi.m_height * heightPercent);
            break;
        case OP_RESIZE_BOTTOM_LEFT:
            newSsroi.m_width =  (int)((float)m_currentRoi.m_width * widthPercent);
            newSsroi.m_height = (int)((float)m_currentRoi.m_height * heightPercent);
            newSsroi.m_offsetX =  (int)((float)m_currentRoi.m_width * offsetXPercent);
            break;
        case OP_RESIZE_TOP:
            newSsroi.m_height = (int)((float)m_currentRoi.m_height * heightPercent);
            newSsroi.m_offsetY =  (int)((float)m_currentRoi.m_height * offsetYPercent);
            break;
        case OP_RESIZE_RIGHT:
            newSsroi.m_width =  (int)((float)m_currentRoi.m_width * widthPercent);
            break;
        case OP_RESIZE_BOTTOM:
            newSsroi.m_height = (int)((float)m_currentRoi.m_height * heightPercent);
            break;
        case OP_RESIZE_LEFT:
            newSsroi.m_width =  (int)((float)m_currentRoi.m_width * widthPercent);
            newSsroi.m_offsetX =  (int)((float)m_currentRoi.m_width * offsetXPercent);
            break;
        case OP_NONE:
        default:
            break;
        }

        //
        // Step 4
        //      Set this SSROI in the camera.  Note that the camera may make an adjustment to our SSROI

        PxLAutoLock lock(&gCameraLock);

        bPreviewRestoreRequired = gCamera->previewing();
        bStreamRestoreRequired = gCamera->streaming() && ! gCamera->triggering();
        if (bPreviewRestoreRequired) gCamera->pausePreview();
        if (bStreamRestoreRequired)gCamera->stopStream();

        PXL_RETURN_CODE rc = gCamera->setRoiValue(SharpnessScoreRoi, newSsroi);
        if (API_SUCCESS(rc))
        {
            if (rc == ApiSuccessParametersChanged)
            {
                // The camera had to adjust the parameters to make them work, read it back to get the correct values.
                rc = gCamera->getRoiValue(SharpnessScoreRoi, &newSsroi);
            }
        }
        if (API_SUCCESS(rc))
        {
            m_ssroi = newSsroi;
        }

        //
        // Step 5
        //      Update our controls to indicate we are using the new ROI

        // Step 5.1
        //      Update the ROI edits
        char cValue[40];

        sprintf (cValue, "%d",newSsroi.m_offsetX);
        gtk_entry_set_text (GTK_ENTRY (m_offsetX), cValue);
        sprintf (cValue, "%d",newSsroi.m_offsetY);
        gtk_entry_set_text (GTK_ENTRY (m_offsetY), cValue);
        sprintf (cValue, "%d",newSsroi.m_width);
        gtk_entry_set_text (GTK_ENTRY (m_width), cValue);
        sprintf (cValue, "%d",newSsroi.m_height);
        gtk_entry_set_text (GTK_ENTRY (m_height), cValue);

        //
        // Step 5.2
        //      Update the SSROI dropdown

        // Temporarily block the handler for ROI selection
        g_signal_handlers_block_by_func (gLensTab->m_ssroiCombo, (gpointer)NewSsroiSelected, NULL);

        // If we are no longer using the 'custom' ROI at the end of our dropdown, remove it.
        if (m_usingNonstandardSsroi)
        {
            gtk_combo_box_text_remove (GTK_COMBO_BOX_TEXT(gLensTab->m_ssroiCombo),
                                       --gLensTab->m_numSsroisSupported);
            gLensTab->m_supportedSsrois.pop_back();
            gLensTab->m_usingNonstandardSsroi = false;
        }

        int newSelectionIndex;
        std::vector<PXL_ROI>::iterator it;
        it = find (gLensTab->m_supportedSsrois.begin(), gLensTab->m_supportedSsrois.end(), newSsroi);
        if (it == gLensTab->m_supportedSsrois.end())
        {
            // This is a new 'non standard' ROI.  Add it to the end
            newSelectionIndex = gLensTab->m_numSsroisSupported;
            sprintf (cValue, "%d x %d", newSsroi.m_width, newSsroi.m_height);
            gtk_combo_box_text_insert_text (GTK_COMBO_BOX_TEXT(gLensTab->m_ssroiCombo),
                                            gLensTab->m_numSsroisSupported++,
                                            cValue);
            gLensTab->m_supportedSsrois.push_back(newSsroi);
            gLensTab->m_usingNonstandardSsroi = true;
        } else {
            // the user 'created' a standard ROI
            newSelectionIndex = it - gLensTab->m_supportedSsrois.begin();
        }
        gtk_combo_box_set_active (GTK_COMBO_BOX(gLensTab->m_ssroiCombo),newSelectionIndex);
        // Unblock the handler for SSROI selection
        g_signal_handlers_unblock_by_func (gLensTab->m_ssroiCombo, (gpointer)NewSsroiSelected, NULL);
    }

    // Reflect these adjustments in the ROI button
    loadRoiImage (bStreamRestoreRequired);
    updateSsroiButton();
    if (bStreamRestoreRequired) gCamera->startStream();
    if (bPreviewRestoreRequired) gCamera->playPreview();

    m_currentOp = OP_NONE;
}

// Set the size of the SSROI button, including putting in the label if there is room
void PxLLens::setSsroiButtonSize ()
{
    bool bLabel;  // true if we want the ROI to have a label

     bLabel = (m_ssroiButtonRoi.m_width >= SSROI_BUTTON_LABEL_THRESHOLD_WIDTH) &&
              (m_ssroiButtonRoi.m_height >= SSROI_BUTTON_LABEL_THRESHOLD_HEIGHT);
     gtk_button_set_label (GTK_BUTTON(m_ssroiButton), bLabel ? SsroiLabel: "");
     gtk_widget_set_size_request (m_ssroiButton, m_ssroiButtonRoi.m_width, m_ssroiButtonRoi.m_height);
}

/* ---------------------------------------------------------------------------
 * --   gtk thread callbacks - used to update controls
 * ---------------------------------------------------------------------------
 */

// Indicate that the refresh is no longer outstanding, it has completed.
static gboolean RefreshComplete (gpointer pData)
{
    PxLLens *pLens = (PxLLens *)pData;

    pLens->m_numRefreshRequestsOutstanding--;
    return false;
}

//
// Make ROI meaningless
static gboolean SsroiDeactivate (gpointer pData)
{
    PxLLens *pLens = (PxLLens *)pData;

    gtk_widget_set_sensitive (pLens->m_ssroiCombo, false);
    gtk_widget_set_sensitive (pLens->m_offsetX, false);
    gtk_widget_set_sensitive (pLens->m_offsetY, false);
    gtk_widget_set_sensitive (pLens->m_width, false);
    gtk_widget_set_sensitive (pLens->m_height, false);
    gtk_widget_set_sensitive (pLens->m_center, false);

    pLens->m_roiBuf = NULL; // Don't use the current FFOV image

    pLens->loadRoiImage();
    pLens->updateSsroiButton();

    return false;  //  Only run once....
}

//
// Assert all of the ROI controls
static gboolean SsroiActivate (gpointer pData)
{
    PxLLens *pLens = (PxLLens *)pData;

    bool settable = false;

    //
    // Step 0
    //      Read the default cursor.
    pLens->m_originalCursor = gdk_window_get_cursor (gtk_widget_get_window (pLens->m_roiImage));

    PxLAutoLock lock(&gCameraLock);

    if (gCamera)
    {
        pLens->m_numSsroisSupported = 0;
        pLens->m_supportedSsrois.clear();
        pLens->m_usingNonstandardSsroi = false;
        gtk_combo_box_text_remove_all (GTK_COMBO_BOX_TEXT(pLens->m_ssroiCombo));

        if (gCamera->supported(FEATURE_SHARPNESS_SCORE))
        {
            settable = gCamera->settable (FEATURE_SHARPNESS_SCORE);

            PXL_ROI minSsroi, maxSsroi, currentSsroi, currentRoi;
            PXL_RETURN_CODE rc  = gCamera->getRoiValue (SharpnessScoreRoi, &currentSsroi);
            PXL_RETURN_CODE rc2 = gCamera->getRoiValue (FrameRoi, &currentRoi);

            if (API_SUCCESS(rc && rc2))
            {
                char cValue[40];

                //
                // Step 1
                //      ROI Size and location edits
                sprintf (cValue, "%d",currentSsroi.m_offsetX);
                gtk_entry_set_text (GTK_ENTRY (pLens->m_offsetX), cValue);
                sprintf (cValue, "%d",currentSsroi.m_offsetY);
                gtk_entry_set_text (GTK_ENTRY (pLens->m_offsetY), cValue);
                sprintf (cValue, "%d",currentSsroi.m_width);
                gtk_entry_set_text (GTK_ENTRY (pLens->m_width), cValue);
                sprintf (cValue, "%d",currentSsroi.m_height);
                gtk_entry_set_text (GTK_ENTRY (pLens->m_height), cValue);

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
                    //      Add all of the standard SSROIS that are larger than min, and smaller than max, and are supported
                    //      by the camera
                    float maxSs = 1.0;
                    rc = gCamera->getRoiRange (SharpnessScoreRoi, &minSsroi, &maxSsroi, false, &maxSs);
                    maxSsroi.m_offsetX = maxSsroi.m_offsetY = 0;  // Offests are 0 for maxSsroi
                    if (API_SUCCESS(rc))
                    {
                        // record the maxRoi, to avoid having to read it a bunch of times.
                        pLens->m_maxSsroi   = maxSsroi;
                        pLens->m_maxSs      = maxSs;
                        pLens->m_currentRoi = currentRoi;
                        pLens->m_roiAspectRatio = (float)currentRoi.m_width / (float)currentRoi.m_height;

                        // Step 2.3
                        //      If we are rotating 90 or 270 degrees, we need to flip the 'standard' ROIs
                        bool  transpose = false;
                        float rotation = 0.0;
                        rc = gCamera->getValue (FEATURE_ROTATE, &rotation);
                        if (API_SUCCESS (rc) && (rotation == 90.0 || rotation == 270.0)) transpose = true;
                        for (int i=0; i < (int)PxLStandardRois.size(); i++)
                        {
                            if (PxLStandardRois[i] < minSsroi) continue;
                            if (PxLStandardRois[i] > maxSsroi) break;
                            if (PxLStandardRois[i] > currentRoi) break;
                            //
                            // 2018-01-25
                            //     As an 'optimzation, don't bother checking each of the indivdual ROIs, simply add
                            //     each of the standard ones (that will fit).  This menas that the times in the list
                            //     might not be supported EXACTLY in the camera; the camera may need to adjust them
                            //     to make it fit.
                            //rc = gCamera->setRoiValue (SharpnessScoreRoi, PxLStandardRois[i]);
                            if (API_SUCCESS(rc))
                            {
                                //if (PxLStandardRois[i] != currentSsroi) restoreRequired = true;
                                PXL_ROI adjustedSsroi = PxLStandardRois[i];
                                if (transpose) adjustedSsroi.rotateClockwise(rotation);
                                //if (rc == ApiSuccessParametersChanged)
                                //{
                                //    rc = gCamera->getRoiValue (SharpnessScoreRoi, &adjustedRoi);
                                //}
                                if (API_SUCCESS(rc))
                                {
                                    sprintf (cValue, "%d x %d", adjustedSsroi.m_width, adjustedSsroi.m_height);
                                    gtk_combo_box_text_insert_text (GTK_COMBO_BOX_TEXT(pLens->m_ssroiCombo),
                                                                    pLens->m_numSsroisSupported++,
                                                                    cValue);
                                    pLens->m_supportedSsrois.push_back(adjustedSsroi);
                                }
                            }
                        }

                        // Step 2.3
                        //      If it's not already there, add the max ROI to the end of the list
                        if (pLens->m_numSsroisSupported == 0 || pLens->m_supportedSsrois[pLens->m_numSsroisSupported-1] != maxSsroi)
                        {
                            sprintf (cValue, "%d x %d", maxSsroi.m_width, maxSsroi.m_height);
                            gtk_combo_box_text_insert_text (GTK_COMBO_BOX_TEXT(pLens->m_ssroiCombo),
                                                            pLens->m_numSsroisSupported++,
                                                            cValue);
                            pLens->m_supportedSsrois.push_back(maxSsroi);
                        }

                        // Step 2.4
                        //      If the current ROI isn't one of the standard one from the list, add this 'custom' one
                        //      to the end.
                        if (find (pLens->m_supportedSsrois.begin(),
                                  pLens->m_supportedSsrois.end(),
                                  currentSsroi) == pLens->m_supportedSsrois.end())
                        {
                            sprintf (cValue, "%d x %d", currentSsroi.m_width, currentSsroi.m_height);
                            gtk_combo_box_text_insert_text (GTK_COMBO_BOX_TEXT(pLens->m_ssroiCombo),
                                                            pLens->m_numSsroisSupported++,
                                                            cValue);
                            pLens->m_supportedSsrois.push_back(currentSsroi);
                            pLens->m_usingNonstandardSsroi = true;
                        }

                        // Step 2.5
                        //      If we changed the ROI, restore the original one
                        if (restoreRequired)
                        {
                            rc = gCamera->setRoiValue (SharpnessScoreRoi, currentSsroi);
                        }
                    }

                    // Step 3
                    //      Mark the current ROI as 'selected'
                    for (int i=0; i<(int)pLens->m_supportedSsrois.size(); i++)
                    {
                        if (pLens->m_supportedSsrois[i] != currentSsroi) continue;
                        gtk_combo_box_set_active (GTK_COMBO_BOX(pLens->m_ssroiCombo),i);
                        break;
                    }

                    // Step 4
                    //      Remember the current ROI
                    pLens->m_ssroi = currentSsroi;

                }
            }
        }
    }

    // Update the FFOV image and ROI button
    pLens->loadRoiImage();
    pLens->updateSsroiButton ();

    // ROI controls
    gtk_widget_set_sensitive (pLens->m_offsetX, settable);
    gtk_widget_set_sensitive (pLens->m_offsetY, settable);
    gtk_widget_set_sensitive (pLens->m_width, settable);
    gtk_widget_set_sensitive (pLens->m_height, settable);

    gtk_widget_set_sensitive (pLens->m_center, settable);

    //ROI drop-down
    gtk_widget_set_sensitive (pLens->m_ssroiCombo, settable);

    return false;  //  Only run once....
}

static gboolean SsUpdate (gpointer pData)
{
    PxLLens *pLens = (PxLLens *)pData;

    // As per C-OEM on Windows, and P-Cap, we represent the sharpness score
    // as a percentage of the maximum, multiplied by a constant 10,000 to make the
    // number more human readable

    float ssToDisplay = (pLens->m_ssLast * 10000.0) / pLens->m_maxSs;
    char cValue[40];

    sprintf (cValue, "%f",ssToDisplay);
    gtk_entry_set_text (GTK_ENTRY (pLens->m_sharpnessScore), cValue);

    return false;  //  Only run once....
}

static void SsFrameCallback (float sharpnessScore)
{
    if (!gLensTab) return;
    if (gLensTab->m_maxSs == 0) return;

    gLensTab->m_ssLast = sharpnessScore;
    gdk_threads_add_idle ((GSourceFunc)SsUpdate, gLensTab);
}

//
// Make Controller controls meaningless
static gboolean ControllerDeactivate (gpointer pData)
{
    PxLLens *pLens = (PxLLens *)pData;

    gtk_widget_set_sensitive (pLens->m_controllerCombo, false);

    return false;  //  Only run once....
}

//
// Assert all of the controller controls
static gboolean ControllerActivate (gpointer pData)
{
    PxLLens *pLens = (PxLLens *)pData;

    // Not currently supported
    gtk_widget_set_sensitive (pLens->m_controllerCombo, false);

    return false;  //  Only run once....
}

//
// Make focus controls meaningless
static gboolean FocusDeactivate (gpointer pData)
{
    PxLLens *pLens = (PxLLens *)pData;

    pLens->m_focusSlider->deactivate();
    gtk_widget_set_sensitive (pLens->m_focusOneTime, false);
    gtk_widget_set_sensitive (pLens->m_focusAutoMin, false);
    gtk_widget_set_sensitive (pLens->m_focusSetAutoMin, false);
    gtk_widget_set_sensitive (pLens->m_focusAutoMax, false);
    gtk_widget_set_sensitive (pLens->m_focusSetAutoMax, false);

    if (gCamera) gCamera->m_poller->pollRemove(focusFuncs);

    return false;  //  Only run once....
}

//
// Assert all of the focus controls
static gboolean FocusActivate (gpointer pData)
{
    PxLLens *pLens = (PxLLens *)pData;

    bool oneTimeEnable = false;
    bool focusSupported = false;
    bool supportsAutoLimits = false;

    PxLAutoLock lock(&gCameraLock);

    if (gCamera)
    {
        if (gCamera->supported(FEATURE_FOCUS))
        {
            focusSupported = true;

            float min, max, value;

            if (gCamera->oneTimeSuppored(FEATURE_FOCUS)) oneTimeEnable = true;

            //pLens->m_focusSlider->activate(true);
            gCamera->getRange(FEATURE_FOCUS, &min, &max);
            pLens->m_focusSlider->setRange(min, max);
            gCamera->getValue(FEATURE_FOCUS, &value);
            pLens->m_focusSlider->setValue(value);

            if (oneTimeEnable && gCamera->numParametersSupported(FEATURE_FOCUS) >= 2)
            {
                char cValue[40];

                supportsAutoLimits = true;

                //gCamera->getAutoLimits(FEATURE_FOCUS, &min, &max);
                gCamera->getRange(FEATURE_FOCUS, &min, &max);
                sprintf (cValue, "%6.0f",min);
                gtk_label_set_text (GTK_LABEL (pLens->m_focusAutoMin), cValue);
                sprintf (cValue, "%6.0f",max);
                gtk_label_set_text (GTK_LABEL (pLens->m_focusAutoMax), cValue);
            }
        }
    }

    gtk_widget_set_sensitive (pLens->m_focusOneTime, focusSupported && oneTimeEnable);
    pLens->m_focusSlider->activate (focusSupported);

    gtk_widget_set_sensitive (pLens->m_focusSetAutoMin, supportsAutoLimits);
    gtk_widget_set_sensitive (pLens->m_focusSetAutoMax, supportsAutoLimits);

    return false;  //  Only run once....
}

//
// Called periodically when doing auto focus updates -- reads the current value
PXL_RETURN_CODE GetCurrentFocus()
{
    PXL_RETURN_CODE rc = ApiSuccess;

    PxLAutoLock lock(&gCameraLock);
    if (gCamera && gLensTab)
    {
        // It's safe to assume the camera supports focus, as this function will not be called
        // otherwise.  If we were to check via pCamera->supported (FEATURE_FOCUS) or
        // pCamera->oneTimeSupported (FEATURE_FOCUS), then that will perform a PxLGetCameraFeatures,
        // which is a lot of work for not.
        float focus = 0.0;
        rc = gCamera->getValue(FEATURE_FOCUS, &focus);
        if (API_SUCCESS(rc)) gLensTab->m_focusLast = focus;
    }

    return rc;
}

//
// Called periodically when doing continuous exposure updates -- updates the user controls
void UpdateFocusControls()
{
    if (gCamera && gLensTab)
    {
        PxLAutoLock lock(&gCameraLock);

        gLensTab->m_focusSlider->setValue(gLensTab->m_focusLast);

        bool onetimeFocusOn = false;
        if (gCamera->m_poller->polling (focusFuncs))
        {
            gCamera->getOnetimeAuto (FEATURE_FOCUS, &onetimeFocusOn);
        }
        if (!onetimeFocusOn)
        {
            // No need to poll any longer
            gCamera->m_poller->pollRemove(focusFuncs);
        }
    }
}

//
// Make zoom controls meaningless
static gboolean ZoomDeactivate (gpointer pData)
{
    PxLLens *pLens = (PxLLens *)pData;

    pLens->m_zoomSlider->deactivate();
    gtk_widget_set_sensitive (pLens->m_zoomAssertMin, false);
    gtk_widget_set_sensitive (pLens->m_zoomAssertMax, false);

    return false;  //  Only run once....
}

//
// Assert all of the zoom controls
static gboolean ZoomActivate (gpointer pData)
{
    PxLLens *pLens = (PxLLens *)pData;

    // Not currently supported
    pLens->m_zoomSlider->deactivate();
    gtk_widget_set_sensitive (pLens->m_zoomAssertMin, false);
    gtk_widget_set_sensitive (pLens->m_zoomAssertMax, false);

    return false;  //  Only run once....
}

/* ---------------------------------------------------------------------------
 * --   Control functions from the Glade project
 * ---------------------------------------------------------------------------
 */

extern "C" void NewSsroiSelected
    (GtkWidget* widget, GdkEventExpose* event, gpointer userdata )
{
    if (! gCamera || ! gCamera) return;
    if (gLensTab->m_numRefreshRequestsOutstanding) return;

    PxLAutoLock lock(&gCameraLock);

    int roiIndex = gtk_combo_box_get_active (GTK_COMBO_BOX(gLensTab->m_ssroiCombo));
    // This routine may have been triggered by the user changing the custom ROI value, in
    // which case it is gone.  That instance is handled completely by the edit field handler
    if (roiIndex < 0 || roiIndex >= (int)gLensTab->m_supportedSsrois.size()) return;

    PXL_ROI newSsroi = gLensTab->m_supportedSsrois[roiIndex];

    // Try to keep the new SSROI centered on the old SSROI.  We need to read the SSROI limits
    // to determine this.
    PXL_RETURN_CODE rc;

    int oldCenterX = gLensTab->m_ssroi.m_offsetX + (gLensTab->m_ssroi.m_width/2);
    int oldCenterY = gLensTab->m_ssroi.m_offsetY + (gLensTab->m_ssroi.m_height/2);
    int newOffsetX = oldCenterX - newSsroi.m_width/2;
    int newOffsetY = oldCenterY - newSsroi.m_height/2;

    // However, we may need to move our center if the new offset does not fit
    if (newOffsetX < 0) newOffsetX = 0;
    if (newOffsetY < 0) newOffsetY = 0;
    if (newOffsetX + newSsroi.m_width > gLensTab->m_currentRoi.m_width) newOffsetX = gLensTab->m_currentRoi.m_width - newSsroi.m_width;
    if (newOffsetY + newSsroi.m_height > gLensTab->m_currentRoi.m_height) newOffsetY = gLensTab->m_currentRoi.m_height - newSsroi.m_height;

    newSsroi.m_offsetX = newOffsetX;
    newSsroi.m_offsetY = newOffsetY;

    rc = gCamera->setRoiValue(SharpnessScoreRoi, newSsroi);
    if (API_SUCCESS(rc))
    {
        if (rc == ApiSuccessParametersChanged)
        {
            // The camera had to adjust the parameters to make them work, read it back to get the correct values.
            rc = gCamera->getRoiValue(SharpnessScoreRoi, &newSsroi);
        }
    }
    if (API_SUCCESS(rc))
    {
        gLensTab->m_ssroi = newSsroi;

        // Update our controls to indicate we are using the new ROI
        char cValue[40];

        sprintf (cValue, "%d",newSsroi.m_offsetX);
        gtk_entry_set_text (GTK_ENTRY (gLensTab->m_offsetX), cValue);
        sprintf (cValue, "%d",newSsroi.m_offsetY);
        gtk_entry_set_text (GTK_ENTRY (gLensTab->m_offsetY), cValue);
        sprintf (cValue, "%d",newSsroi.m_width);
        gtk_entry_set_text (GTK_ENTRY (gLensTab->m_width), cValue);
        sprintf (cValue, "%d",newSsroi.m_height);
        gtk_entry_set_text (GTK_ENTRY (gLensTab->m_height), cValue);

        // If we are no longer using the 'custom' ROI at the end of our dropdown, remove it.
        if (gLensTab->m_usingNonstandardSsroi && roiIndex != gLensTab->m_numSsroisSupported-1)
        {
            gtk_combo_box_text_remove (GTK_COMBO_BOX_TEXT(gLensTab->m_ssroiCombo),
                                       --gLensTab->m_numSsroisSupported);
            gLensTab->m_supportedSsrois.pop_back();
            gLensTab->m_usingNonstandardSsroi = false;
        }

        // We changed the SS ROI, so the max SS value has changed too.  Re-read it.
        float maxSs = 1.0;
        PXL_ROI minSsroi, maxSsroi;
        rc = gCamera->getRoiRange (SharpnessScoreRoi, &minSsroi, &maxSsroi, false, &maxSs);
        if (API_SUCCESS(rc)) gLensTab->m_maxSs  = maxSs;

        // Update the SSROI button
        gLensTab->loadRoiImage();
        gLensTab->updateSsroiButton();
    }
}

extern "C" void SsroiOffsetXValueChanged
    (GtkWidget* widget, GdkEventExpose* event, gpointer userdata )
{
    if (! gCamera || ! gLensTab) return;
    if (gLensTab->m_numRefreshRequestsOutstanding) return;

    PxLAutoLock lock(&gCameraLock);

    int newOffsetX = atoi (gtk_entry_get_text (GTK_ENTRY (gLensTab->m_offsetX)));
    PXL_RETURN_CODE rc;

    PXL_ROI newSsroi = gLensTab->m_ssroi;

    if (newOffsetX + newSsroi.m_width > gLensTab->m_currentRoi.m_width) newOffsetX = gLensTab->m_currentRoi.m_width - newSsroi.m_width;
    newSsroi.m_offsetX = newOffsetX;

    rc = gCamera->setRoiValue(SharpnessScoreRoi, newSsroi);
    if (API_SUCCESS (rc))
    {
        // Read it back again, just in case the camera did some 'tuning' of the values
        rc = gCamera->getRoiValue(SharpnessScoreRoi, &newSsroi);

        if (API_SUCCESS (rc))
        {
            gLensTab->m_ssroi = newSsroi;
        }

        // We changed the SS ROI, so the max SS value has changed too.  Re-read it.
        float maxSs = 1.0;
        PXL_ROI minSsroi, maxSsroi;
        rc = gCamera->getRoiRange (SharpnessScoreRoi, &minSsroi, &maxSsroi, false, &maxSs);
        if (API_SUCCESS(rc)) gLensTab->m_maxSs  = maxSs;

        // Update the ROI button
        gLensTab->loadRoiImage();
        gLensTab->updateSsroiButton();
    }

    // Reassert the current value even if we did not succeed in setting it
    char cValue[40];
    sprintf (cValue, "%d",gLensTab->m_ssroi.m_offsetX);
    gtk_entry_set_text (GTK_ENTRY (gLensTab->m_offsetX), cValue);
}

extern "C" void SsroiOffsetYValueChanged
    (GtkWidget* widget, GdkEventExpose* event, gpointer userdata )
{
    if (! gCamera || ! gLensTab) return;
    if (gLensTab->m_numRefreshRequestsOutstanding) return;

    PxLAutoLock lock(&gCameraLock);

    int newOffsetY = atoi (gtk_entry_get_text (GTK_ENTRY (gLensTab->m_offsetY)));
    PXL_RETURN_CODE rc;

    PXL_ROI newSsroi = gLensTab->m_ssroi;

    if (newOffsetY + newSsroi.m_height > gLensTab->m_currentRoi.m_height) newOffsetY = gLensTab->m_currentRoi.m_height - newSsroi.m_height;
    newSsroi.m_offsetY = newOffsetY;

    rc = gCamera->setRoiValue(SharpnessScoreRoi, newSsroi);
    if (API_SUCCESS (rc))
    {
        // Read it back again, just in case the camera did some 'tuning' of the values
        rc = gCamera->getRoiValue(SharpnessScoreRoi, &newSsroi);

        if (API_SUCCESS (rc))
        {
            gLensTab->m_ssroi = newSsroi;
        }

        // We changed the SS ROI, so the max SS value has changed too.  Re-read it.
        float maxSs = 1.0;
        PXL_ROI minSsroi, maxSsroi;
        rc = gCamera->getRoiRange (SharpnessScoreRoi, &minSsroi, &maxSsroi, false, &maxSs);
        if (API_SUCCESS(rc)) gLensTab->m_maxSs  = maxSs;

        // Update the ROI button
        gLensTab->loadRoiImage();
        gLensTab->updateSsroiButton();
    }

    // Reassert the current value even if we did not succeed in setting it
    char cValue[40];
    sprintf (cValue, "%d",gLensTab->m_ssroi.m_offsetY);
    gtk_entry_set_text (GTK_ENTRY (gLensTab->m_offsetY), cValue);
}

extern "C" void SsroiWidthValueChanged
    (GtkWidget* widget, GdkEventExpose* event, gpointer userdata )
{
    if (! gCamera || ! gLensTab) return;
    if (gLensTab->m_numRefreshRequestsOutstanding) return;

    PxLAutoLock lock(&gCameraLock);

    char cValue[40];

    // NOTE, this procedure will adjust the offsetX value, if doing so will accommodate the user
    // suppled width
    int newWidth = atoi (gtk_entry_get_text (GTK_ENTRY (gLensTab->m_width)));
    int newOffsetX = gLensTab->m_ssroi.m_offsetX;

    PXL_RETURN_CODE rc;

    PXL_ROI newSsroi = gLensTab->m_ssroi;

    // Trim the width if it is too big
    newWidth = min (newWidth, gLensTab->m_maxSsroi.m_width);

    // Trim the offset if we need to to accomodate the width
    if (newWidth + newSsroi.m_offsetX > gLensTab->m_currentRoi.m_width)
    {
        newOffsetX = gLensTab->m_currentRoi.m_width - newWidth;
    }
    if (newOffsetX >= 0)
    {
        // Looks like we found a width and offsetX that should work
        newSsroi.m_width = newWidth;
        newSsroi.m_offsetX = newOffsetX;

        rc = gCamera->setRoiValue(SharpnessScoreRoi, newSsroi);
        if (API_SUCCESS (rc))
        {
            // Read it back again, just in case the camera did some 'tuning' of the values
            rc = gCamera->getRoiValue(SharpnessScoreRoi, &newSsroi);
            if (API_SUCCESS (rc))
            {
                gLensTab->m_ssroi = newSsroi;

                // Update the controls with the new ROI width (and offsetx, as it may have changed.  Note that
                // the actual edit field is done later, even on failure.
                sprintf (cValue, "%d",gLensTab->m_ssroi.m_offsetX);
                gtk_entry_set_text (GTK_ENTRY (gLensTab->m_offsetX), cValue);
                if (gLensTab->m_usingNonstandardSsroi)
                {
                    gtk_combo_box_text_remove (GTK_COMBO_BOX_TEXT(gLensTab->m_ssroiCombo),
                                               --gLensTab->m_numSsroisSupported);
                    gLensTab->m_supportedSsrois.pop_back();
                    gLensTab->m_usingNonstandardSsroi = false;
                }

                int newSelectionIndex;
                std::vector<PXL_ROI>::iterator it;
                it = find (gLensTab->m_supportedSsrois.begin(), gLensTab->m_supportedSsrois.end(), newSsroi);
                if (it == gLensTab->m_supportedSsrois.end())
                {
                    // This is a new 'non standard' ROI.  Add it to the end
                    newSelectionIndex = gLensTab->m_numSsroisSupported;
                    sprintf (cValue, "%d x %d", newSsroi.m_width, newSsroi.m_height);
                    gtk_combo_box_text_insert_text (GTK_COMBO_BOX_TEXT(gLensTab->m_ssroiCombo),
                                                    gLensTab->m_numSsroisSupported++,
                                                    cValue);
                    gLensTab->m_supportedSsrois.push_back(newSsroi);
                    gLensTab->m_usingNonstandardSsroi = true;
                } else {
                    // the user 'created' a standard ROI
                    newSelectionIndex = it - gLensTab->m_supportedSsrois.begin();
                }
                gtk_combo_box_set_active (GTK_COMBO_BOX(gLensTab->m_ssroiCombo),newSelectionIndex);

            }

            // We changed the SS ROI, so the max SS value has changed too.  Re-read it.
            float maxSs = 1.0;
            PXL_ROI minSsroi, maxSsroi;
            rc = gCamera->getRoiRange (SharpnessScoreRoi, &minSsroi, &maxSsroi, false, &maxSs);
            if (API_SUCCESS(rc)) gLensTab->m_maxSs  = maxSs;

            // Update the ROI button
            gLensTab->loadRoiImage();
            gLensTab->updateSsroiButton();
        }
    }

    // Reassert the current value even if we did not succeed in setting it
    sprintf (cValue, "%d",gLensTab->m_ssroi.m_width);
    gtk_entry_set_text (GTK_ENTRY (gLensTab->m_width), cValue);
}

extern "C" void SsroiHeightValueChanged
    (GtkWidget* widget, GdkEventExpose* event, gpointer userdata )
{
    if (! gCamera || ! gLensTab) return;
    if (gLensTab->m_numRefreshRequestsOutstanding) return;

    PxLAutoLock lock(&gCameraLock);

    char cValue[40];

    // NOTE, this procedure will adjust the offsetY value, if doing so will accommodate the user
    // suppled height
    int newHeight = atoi (gtk_entry_get_text (GTK_ENTRY (gLensTab->m_height)));
    int newOffsetY = gLensTab->m_ssroi.m_offsetY;

    PXL_RETURN_CODE rc;
    PXL_ROI newSsroi = gLensTab->m_ssroi;

    // trim the height if needed
    newHeight = min (newHeight, gLensTab->m_maxSsroi.m_height);
    if (newHeight + newSsroi.m_offsetY > gLensTab->m_currentRoi.m_height)
    {
        newOffsetY = gLensTab->m_currentRoi.m_height - newHeight;
    }
    if (newOffsetY >= 0)
    {
        // Looks like we found a width and offsetX that should work
        newSsroi.m_height = newHeight;
        newSsroi.m_offsetY = newOffsetY;

        rc = gCamera->setRoiValue(SharpnessScoreRoi, newSsroi);
        if (API_SUCCESS (rc))
        {
            // Read it back again, just in case the camera did some 'tuning' of the values
            rc = gCamera->getRoiValue(SharpnessScoreRoi, &newSsroi);

            if (API_SUCCESS (rc))
            {
                gLensTab->m_ssroi = newSsroi;

                // Update the controls with the new ROI height (and offsety, as it may have changed.  Note that
                // the actual edit field is done later, even on failure.
                sprintf (cValue, "%d",gLensTab->m_ssroi.m_offsetY);
                gtk_entry_set_text (GTK_ENTRY (gLensTab->m_offsetY), cValue);
                if (gLensTab->m_usingNonstandardSsroi)
                {
                    gtk_combo_box_text_remove (GTK_COMBO_BOX_TEXT(gLensTab->m_ssroiCombo),
                                               --gLensTab->m_numSsroisSupported);
                    gLensTab->m_supportedSsrois.pop_back();
                    gLensTab->m_usingNonstandardSsroi = false;
                }
                int newSelectionIndex;
                std::vector<PXL_ROI>::iterator it;
                it = find (gLensTab->m_supportedSsrois.begin(), gLensTab->m_supportedSsrois.end(), newSsroi);
                if (it == gLensTab->m_supportedSsrois.end())
                {
                    // This is a new 'non standard' ROI.  Add it to the end
                    newSelectionIndex = gLensTab->m_numSsroisSupported;
                    sprintf (cValue, "%d x %d", newSsroi.m_width, newSsroi.m_height);
                    gtk_combo_box_text_insert_text (GTK_COMBO_BOX_TEXT(gLensTab->m_ssroiCombo),
                                                    gLensTab->m_numSsroisSupported++,
                                                    cValue);
                    gLensTab->m_supportedSsrois.push_back(newSsroi);
                    gLensTab->m_usingNonstandardSsroi = true;
                } else {
                    // the user 'created' a standard ROI
                    newSelectionIndex = it - gLensTab->m_supportedSsrois.begin();
                }
                gtk_combo_box_set_active (GTK_COMBO_BOX(gLensTab->m_ssroiCombo),newSelectionIndex);

            }

            // We changed the SS ROI, so the max SS value has changed too.  Re-read it.
            float maxSs = 1.0;
            PXL_ROI minSsroi, maxSsroi;
            rc = gCamera->getRoiRange (SharpnessScoreRoi, &minSsroi, &maxSsroi, false, &maxSs);
            if (API_SUCCESS(rc)) gLensTab->m_maxSs  = maxSs;

            // Update the ROI button
            gLensTab->loadRoiImage();
            gLensTab->updateSsroiButton();
        }
    }

    // Reassert the current value even if we did not succeed in setting it
    sprintf (cValue, "%d",gLensTab->m_ssroi.m_height);
    gtk_entry_set_text (GTK_ENTRY (gLensTab->m_height), cValue);
}

extern "C" void SsroiCenterButtonPressed
    (GtkWidget* widget, GdkEventExpose* event, gpointer userdata )
{
    if (! gCamera || ! gCameraSelectTab || !gLensTab) return;
    if (gLensTab->m_numRefreshRequestsOutstanding) return;

    PxLAutoLock lock(&gCameraLock);

    PXL_RETURN_CODE rc;

    PXL_ROI newSsroi = gLensTab->m_ssroi;

    int newOffsetX = (gLensTab->m_currentRoi.m_width - newSsroi.m_width) / 2;
    int newOffsetY = (gLensTab->m_currentRoi.m_height - newSsroi.m_height) / 2;

    newSsroi.m_offsetX = newOffsetX;
    newSsroi.m_offsetY = newOffsetY;

    rc = gCamera->setRoiValue(SharpnessScoreRoi, newSsroi);
    if (API_SUCCESS (rc))
    {
        // Read it back again, just in case the camera did some 'tuning' of the values
        gCamera->getRoiValue(SharpnessScoreRoi, &newSsroi);

        gLensTab->m_ssroi = newSsroi;

        // Update the controls
        char cValue[40];
        sprintf (cValue, "%d",gLensTab->m_ssroi.m_offsetX);
        gtk_entry_set_text (GTK_ENTRY (gLensTab->m_offsetX), cValue);
        sprintf (cValue, "%d",gLensTab->m_ssroi.m_offsetY);
        gtk_entry_set_text (GTK_ENTRY (gLensTab->m_offsetY), cValue);

        // We changed the SS ROI, so the max SS value has changed too.  Re-read it.
        float maxSs = 1.0;
        PXL_ROI minSsroi, maxSsroi;
        rc = gCamera->getRoiRange (SharpnessScoreRoi, &minSsroi, &maxSsroi, false, &maxSs);
        if (API_SUCCESS(rc)) gLensTab->m_maxSs  = maxSs;

        // Update the ROI button
        gLensTab->loadRoiImage();
        gLensTab->updateSsroiButton();
    }
}

extern "C" bool SsroiButtonPress
    (GtkWidget* widget, GdkEventButton *event )
{
    if (! gCamera || ! gLensTab) return true;
    if (gLensTab->m_numRefreshRequestsOutstanding) return true;

    // Pressing the button will 'start' an operation, and releasing it will end one
    if (event->type == GDK_BUTTON_PRESS)
    {
        gLensTab->startSsroiButtonOperation(widget, event->x, event->y, event->x_root, event->y_root);
    } else {
        gLensTab->finishSsroiButtonOperation(widget);
    }

    return true;
}

extern "C" bool SsroiButtonMotion
    (GtkWidget* widget, GdkEventButton *event )
{
    if (! gCamera || ! gLensTab) return true;
    if (gLensTab->m_numRefreshRequestsOutstanding) return true;

    //
    // Step 1
    //      If we are not performing an operation, than all we need to
    //      do, is set the cursor type
    if (gLensTab->m_currentOp == OP_NONE)
    {
        gdk_window_set_cursor (gtk_widget_get_window (widget),
                               gLensTab->getCursorForOperation ( gLensTab->determineOperationFromMouse (event->x, event->y)));
        return true;
    }

    //
    // Step 2
    //      We are performing some sort of operation IE, the user is dragging the mouse.  We need to redraw
    //      the ROI button as the user does the drag
    int deltaX = (int) (event->x_root -  gLensTab->m_mouseXOnOpStart);
    int deltaY = (int) (event->y_root -  gLensTab->m_mouseYOnOpStart);

    // define some local variables just to make the code more compact and readable
    int *width = &gLensTab->m_ssroiButtonRoi.m_width;
    int *height = &gLensTab->m_ssroiButtonRoi.m_height;
    int *x = &gLensTab->m_ssroiButtonRoi.m_offsetX;
    int *y = &gLensTab->m_ssroiButtonRoi.m_offsetY;
    int xMin = gLensTab->m_ssroiButtonLimits.xMin;
    int xMax = gLensTab->m_ssroiButtonLimits.xMax;
    int yMin = gLensTab->m_ssroiButtonLimits.yMin;
    int yMax = gLensTab->m_ssroiButtonLimits.yMax;
    int widthStart =  gLensTab->m_ssroiButtonOnOpStart.m_width;
    int heightStart = gLensTab->m_ssroiButtonOnOpStart.m_height;
    int xStart =  gLensTab->m_ssroiButtonOnOpStart.m_offsetX;
    int yStart = gLensTab->m_ssroiButtonOnOpStart.m_offsetY;

    switch (gLensTab->m_currentOp)
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
        gLensTab->setSsroiButtonSize();
        break;
    case OP_RESIZE_TOP_RIGHT:
        *y = min (max (yMin, yStart + deltaY), yStart + heightStart - ROI_MIN_HEIGHT);
        gtk_widget_set_margin_top (widget, *y);
        *width = min (max (ROI_MIN_WIDTH, widthStart + deltaX), xMax - xStart);
        *height = min (max (ROI_MIN_HEIGHT, heightStart - deltaY), yStart + heightStart - yMin);
        gLensTab->setSsroiButtonSize();
        break;
    case OP_RESIZE_BOTTOM_RIGHT:
        *width = min (max (ROI_MIN_WIDTH, widthStart + deltaX), xMax - xStart);
        *height = min (max (ROI_MIN_HEIGHT, heightStart + deltaY), yMax - yStart);
        gLensTab->setSsroiButtonSize();
        break;
    case OP_RESIZE_BOTTOM_LEFT:
        *x = min (max (xMin, xStart + deltaX), xStart + widthStart - ROI_MIN_WIDTH);
        gtk_widget_set_margin_left (widget, *x);
        *width = min (max (ROI_MIN_WIDTH, widthStart - deltaX), xStart + widthStart - xMin);
        *height = min (max (ROI_MIN_HEIGHT, heightStart + deltaY), yMax - yStart);
        gLensTab->setSsroiButtonSize();
        break;
    case OP_RESIZE_TOP:
        *y = min (max (yMin, yStart + deltaY), yStart + heightStart - ROI_MIN_HEIGHT);
        gtk_widget_set_margin_top (widget, *y);
        *height = min (max (ROI_MIN_HEIGHT, heightStart - deltaY), yStart + heightStart - yMin);
        gLensTab->setSsroiButtonSize();
        break;
    case OP_RESIZE_RIGHT:
        *width = min ( max (ROI_MIN_WIDTH, widthStart + deltaX), xMax - xStart);
        gLensTab->setSsroiButtonSize();
        break;
    case OP_RESIZE_BOTTOM:
        *height = min ( max (ROI_MIN_HEIGHT, heightStart + deltaY), yMax - yStart);
        gLensTab->setSsroiButtonSize();
        break;
    case OP_RESIZE_LEFT:
        *x = min (max (xMin, xStart + deltaX), xStart + widthStart - ROI_MIN_WIDTH);
        gtk_widget_set_margin_left (widget, *x);
        *width = min (max (ROI_MIN_WIDTH, widthStart - deltaX), xStart + widthStart - xMin);
        gLensTab->setSsroiButtonSize();
        break;
    default:
    case OP_NONE:
        // Just for completeness
        break;
    }

    return true;
}

extern "C" bool SsroiButtonEnter
    (GtkWidget* widget, GdkEventButton *event )
{
    if (! gCamera || ! gLensTab) return true;
    if (gLensTab->m_numRefreshRequestsOutstanding) return true;

    // TEMP PEC +++
    //      NOTE.  I don't think I even need this event.
    // ---
    // If we are performing an operation (the user has the button pressed), then don't take any
    // action on leave/enter -- wait for the user to complete.
    if (gLensTab->m_currentOp != OP_NONE) return TRUE;

    // The user is not performing an operation.
    //gdk_window_set_cursor (gtk_widget_get_window (widget),
    //                       gLensTab->getCursorForOperation (gLensTab->determineOperationFromMouse (event->x, event->y)));

    return true;
}

extern "C" void FocusValueChanged
    (GtkWidget* widget, GdkEventExpose* event, gpointer userdata )
{
    if (! gCamera || ! gCameraSelectTab || !gLensTab) return;
    if (gLensTab->m_numRefreshRequestsOutstanding) return;

    PxLAutoLock lock(&gCameraLock);

    float newFocus;

    newFocus = gLensTab->m_focusSlider->getEditValue();

    gCamera->setValue(FEATURE_FOCUS, newFocus);

    // read it back again to see if the camera accepted it, or perhaps 'rounded it' to a
    // new value
    gCamera->getValue(FEATURE_FOCUS, &newFocus);
    gLensTab->m_focusSlider->setValue(newFocus);
}

extern "C" void FocusScaleChanged
    (GtkWidget* widget, GdkEventExpose* event, gpointer userdata )
{

    if (! gCamera || ! gCameraSelectTab || !gLensTab) return;
    if (gLensTab->m_numRefreshRequestsOutstanding) return;

    // we are only interested in changes to the scale from user input
    if (gLensTab->m_focusSlider->rangeChangeInProgress()) return;
    if (gLensTab->m_focusSlider->setIsInProgress()) return;

    float newFocus;

    PxLAutoLock lock(&gCameraLock);

    newFocus = gLensTab->m_focusSlider->getScaleValue();

    gCamera->setValue(FEATURE_FOCUS, newFocus);
    // read it back again to see if the camera accepted it, or perhaps 'rounded it' to a
    // new value
    gCamera->getValue(FEATURE_FOCUS, &newFocus);
    gLensTab->m_focusSlider->setValue(newFocus);
}

extern "C" void FocusOneTimeButtonPressed
    (GtkWidget* widget, GdkEventExpose* event, gpointer userdata )
{
    if (! gCamera || ! gCameraSelectTab || !gLensTab) return;
    if (!gCamera->oneTimeSuppored(FEATURE_FOCUS)) return;
    if (gLensTab->m_numRefreshRequestsOutstanding) return;

    PxLAutoLock lock(&gCameraLock);

    // Use the limits if they are supported.  We can easily tell this by checking
    // one of the limit buttons to see if it is enabled.
    bool autoLimitsSupported =  gtk_widget_get_sensitive (gLensTab->m_focusSetAutoMin);

    if (autoLimitsSupported)
    {
        float min = atof (gtk_label_get_text (GTK_LABEL (gLensTab->m_focusAutoMin)));
        float max = atof (gtk_label_get_text (GTK_LABEL (gLensTab->m_focusAutoMax)));
        gOnetimeDialog->initiate(FEATURE_FOCUS, 500, min, max); // Pool every 500 ms
    } else {
        gOnetimeDialog->initiate(FEATURE_FOCUS, 500); // Pool every 500 ms
    }

    gOnetimeDialog->initiate(FEATURE_FOCUS, 500); // Pool every 500 ms
    // Also add a poller so that the slider and the edit control also update as the
    // one time is performed.
    gCamera->m_poller->pollAdd(focusFuncs);
}

extern "C" void FocusAutoMinButtonPressed
    (GtkWidget* widget, GdkEventExpose* event, gpointer userdata )
{
    if (! gCamera || ! gCameraSelectTab || !gLensTab) return;
    if (gLensTab->m_numRefreshRequestsOutstanding) return;

    PxLAutoLock lock(&gCameraLock);

    float currentFocus;
    char cValue[40];

    currentFocus = gLensTab->m_focusSlider->getEditValue();

    sprintf (cValue, "%6.0f",currentFocus);
    gtk_label_set_text (GTK_LABEL (gLensTab->m_focusAutoMin), cValue);
}

extern "C" void FocusAutoMaxButtonPressed
    (GtkWidget* widget, GdkEventExpose* event, gpointer userdata )
{
    if (! gCamera || ! gCameraSelectTab || !gLensTab) return;
    if (gLensTab->m_numRefreshRequestsOutstanding) return;

    PxLAutoLock lock(&gCameraLock);

    float currentFocus;
    char cValue[40];

    currentFocus = gLensTab->m_focusSlider->getEditValue();

    sprintf (cValue, "%6.0f",currentFocus);
    gtk_label_set_text (GTK_LABEL (gLensTab->m_focusAutoMax), cValue);
}


