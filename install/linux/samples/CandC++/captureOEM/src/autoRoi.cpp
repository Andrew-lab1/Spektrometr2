
/***************************************************************************
 *
 *     File: autoRoi.cpp
 *
 *     Description:
 *        Controls for the 'AutoRoi' tab  in CaptureOEM.
 */

#include <algorithm>
#include "autoRoi.h"
#include "cameraSelect.h"
#include "camera.h"
#include "captureOEM.h"
#include "preview.h"
#include "controls.h"
#include "stream.h"
#include "video.h"
#include "link.h"
#include "onetime.h"

using namespace std;

extern PxLAutoRoi      *gAutoRoiTab;
extern PxLCameraSelect *gCameraSelectTab;
extern PxLPreview      *gVideoPreviewTab;
extern PxLOnetime      *gOnetimeDialog;
extern PxLControls     *gControlsTab;
#if defined VIDEO_CAPTURE_SUPPORTED
extern PxLVideo        *gVideoTab;
#endif
extern PxLLink         *gLinkTab;

//
// Local prototypes.
//    UI updates can only be done from a gtk thread -- these routines are gtk 'idle' threads
//    and as such are 'UI update safe'. For each 'feature', there there the following functions:
//       . {featureXXX}Deactivate - Makes the controls meaningless (including greying them out)
//       . {featreuXXX}Activate - updates the controls with values from the camera
static gboolean  RefreshComplete (gpointer pData);
static gboolean  AutoroiDeactivate (gpointer pData);
static gboolean  AutoroiActivate (gpointer pData);
static gboolean  OperationsDeactivate (gpointer pData);
static gboolean  OperationsActivate (gpointer pData);

// Prototypes for functions used for features with auto modes (continuous and onetime).
static PXL_RETURN_CODE GetCurrentExposure();
static void UpdateExposureControls();
static const PxLFeaturePollFunctions exposureFuncs (GetCurrentExposure, UpdateExposureControls);
static PXL_RETURN_CODE GetCurrentGain();
static void UpdateGainControls();
static const PxLFeaturePollFunctions gainFuncs (GetCurrentGain, UpdateGainControls);
static PXL_RETURN_CODE GetCurrentWhitebalance();
static void UpdateWhitebalanceControls();
static const PxLFeaturePollFunctions whitebalanceFuncs (GetCurrentWhitebalance, UpdateWhitebalanceControls);

extern "C" void NewAutoroiSelected (GtkWidget* widget, GdkEventExpose* event, gpointer userdata);

extern "C" bool AutoroiButtonPress  (GtkWidget* widget, GdkEventButton *event );
extern "C" bool AutoroiButtonMotion (GtkWidget* widget, GdkEventButton *event );
extern "C" bool AutoroiButtonEnter  (GtkWidget* widget, GdkEventButton *event );

/* ---------------------------------------------------------------------------
 * --   Static data used to construct our dropdowns - Private
 * ---------------------------------------------------------------------------
 */

static const char  AutoroiLabel[] = " Current\nAuto-ROI";


/* ---------------------------------------------------------------------------
 * --   Member functions - Public
 * ---------------------------------------------------------------------------
 */
PxLAutoRoi::PxLAutoRoi (GtkBuilder *builder)
: m_roiBuf(NULL)
, m_roiAspectRatio((float)ROI_AREA_WIDTH / (float)ROI_AREA_HEIGHT)
, m_currentOp(OP_NONE)
{
    //
    // Step 1
    //      Find all of the glade controls

    // Roi
    m_enable = GTK_WIDGET( gtk_builder_get_object( builder, "AutoroiEnable_Checkbox" ) );
    m_autoroiCombo = GTK_WIDGET( gtk_builder_get_object( builder, "Autoroi_Combo" ) );
    m_offsetX = GTK_WIDGET( gtk_builder_get_object( builder, "AutoroiOffsetX_Text" ) );
    m_offsetY = GTK_WIDGET( gtk_builder_get_object( builder, "AutoroiOffsetY_Text" ) );
    m_width = GTK_WIDGET( gtk_builder_get_object( builder, "AutoroiWidth_Text" ) );
    m_height = GTK_WIDGET( gtk_builder_get_object( builder, "AutoroiHeight_Text" ) );
    m_center = GTK_WIDGET( gtk_builder_get_object( builder, "AutoroiCenter_Button" ) );
    m_roiImage = GTK_WIDGET( gtk_builder_get_object( builder, "Autoroi_Image" ) );
    m_autoroiButton = GTK_WIDGET( gtk_builder_get_object( builder, "Autoroi_Button" ) );

    // connect elements from the style sheet, to our builder
    gtk_widget_set_name (m_autoroiButton, "Autoroi_Button_red");

    m_exposure =  GTK_WIDGET( gtk_builder_get_object( builder, "AutoroiExposure_Text" ) );
    m_gain =  GTK_WIDGET( gtk_builder_get_object( builder, "AutoroiGain_Text" ) );
    m_red = GTK_WIDGET( gtk_builder_get_object( builder, "AutoroiRed_Text" ) );
    m_green = GTK_WIDGET( gtk_builder_get_object( builder, "AutoroiGreen_Text" ) );
    m_blue = GTK_WIDGET( gtk_builder_get_object( builder, "AutoroiBlue_Text" ) );

    m_exposureOneTime = GTK_WIDGET( gtk_builder_get_object( builder, "AutoroiExposureOneTime_Button" ) );
    m_gainOneTime = GTK_WIDGET( gtk_builder_get_object( builder, "AutoroiGainOneTime_Button" ) );
    m_whiteBalanceOneTime = GTK_WIDGET( gtk_builder_get_object( builder, "AutoroiWhiteBalanceOneTime_Button" ) );


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
    g_signal_connect (m_autoroiButton, "button_press_event",   G_CALLBACK (AutoroiButtonPress), NULL);
    g_signal_connect (m_autoroiButton, "button_release_event", G_CALLBACK (AutoroiButtonPress), NULL);
    g_signal_connect (m_autoroiButton, "motion_notify_event",  G_CALLBACK (AutoroiButtonMotion), NULL);
    g_signal_connect (m_autoroiButton, "enter_notify_event",   G_CALLBACK (AutoroiButtonEnter), NULL);
    g_signal_connect (m_autoroiButton, "leave_notify_event",   G_CALLBACK (AutoroiButtonEnter), NULL);

    gtk_widget_set_events (m_autoroiButton,
                           GDK_ENTER_NOTIFY_MASK | GDK_LEAVE_NOTIFY_MASK |
                           GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK |
                           GDK_POINTER_MOTION_MASK | GDK_POINTER_MOTION_HINT_MASK);
}

PxLAutoRoi::~PxLAutoRoi ()
{
}

void PxLAutoRoi::refreshRequired (bool noCamera)
{
    m_roiBuf = NULL;

    if (IsActiveTab (AutoRoiTab))
    {
        if (noCamera)
        {
            // If I am the active tab, then grey out everything
            gdk_threads_add_idle ((GSourceFunc)AutoroiDeactivate, this);
            gdk_threads_add_idle ((GSourceFunc)OperationsDeactivate, this);
        } else {
            // If I am the active tab, then refresh everything
            gdk_threads_add_idle ((GSourceFunc)AutoroiActivate, this);
            gdk_threads_add_idle ((GSourceFunc)OperationsActivate, this);
        }
        gdk_threads_add_idle ((GSourceFunc)RefreshComplete, this);
        m_numRefreshRequestsOutstanding++;
    } else {
        // If we are not the active tab, only bump the m_numRefreshRequestsOutstanding if there is not
        // one outstanding already; RefreshComplete will be scheduled when the tab becomes active
        if (!m_numRefreshRequestsOutstanding)m_numRefreshRequestsOutstanding++;
    }
}

void PxLAutoRoi::activate()
{
    // I have become the active tab.

    if (gCamera)
    {
        if (m_numRefreshRequestsOutstanding)
        {
            gdk_threads_add_idle ((GSourceFunc)AutoroiActivate, this);
            gdk_threads_add_idle ((GSourceFunc)OperationsActivate, this);
        } else {
            // Start the pollers for any feature in the 'continuous' mode.  We know there isn't a one time
            // in progress, because we could not have changed tabs while the onetime dialog is active
            bool continuousCurrentlyOn = false;
            gCamera->getContinuousAuto(FEATURE_EXPOSURE, &continuousCurrentlyOn);
            if (continuousCurrentlyOn) gCamera->m_poller->pollAdd(exposureFuncs);

            continuousCurrentlyOn = false;
            gCamera->getContinuousAuto(FEATURE_GAIN, &continuousCurrentlyOn);
            if (continuousCurrentlyOn) gCamera->m_poller->pollAdd(gainFuncs);
        }
    } else {
        gdk_threads_add_idle ((GSourceFunc)AutoroiDeactivate, this);
        gdk_threads_add_idle ((GSourceFunc)OperationsDeactivate, this);
    }

    m_numRefreshRequestsOutstanding = 1; // As a safety mechanism, tab activation should assert value, it will be set to 0 when RefreshComplete
    gdk_threads_add_idle ((GSourceFunc)RefreshComplete, this);
}

void PxLAutoRoi::deactivate()
{
    // I am no longer the active tab.

    // Stop the pollers for any feature in continuous mode.  We will start them again if re-activated
    if (gCamera)
    {
        bool continuousCurrentlyOn = false;
        gCamera->getContinuousAuto(FEATURE_EXPOSURE, &continuousCurrentlyOn);
        if (continuousCurrentlyOn) gCamera->m_poller->pollRemove(exposureFuncs);

        continuousCurrentlyOn = false;
        gCamera->getContinuousAuto(FEATURE_GAIN, &continuousCurrentlyOn);
        if (continuousCurrentlyOn) gCamera->m_poller->pollRemove(gainFuncs);
    }
}

// indication that the app has transitioned to/from playing state.
void PxLAutoRoi::playChange (bool playing)
{
    if (IsActiveTab (AutoRoiTab)) loadRoiImage();
}

// Load the ROI background image.  streamInterrupted is true if the stream is supposed to be running, but
// has temporarily been turned off to perform some operations.
void PxLAutoRoi::loadRoiImage(bool streamInterrupted)
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
            m_autoroiButtonLimits.xMax = ROI_AREA_WIDTH;
            m_autoroiButtonLimits.yMax = (int)((float)ROI_AREA_WIDTH / m_roiAspectRatio);
            m_autoroiButtonLimits.xMin = 0;
            m_autoroiButtonLimits.yMin = (ROI_AREA_HEIGHT - m_autoroiButtonLimits.yMax) / 2 ;
            m_autoroiButtonLimits.yMax += m_autoroiButtonLimits.yMin;
        } else {
            // width limited (or unlimited (same aspect ratio)
            m_autoroiButtonLimits.xMax = (int)((float)ROI_AREA_HEIGHT * m_roiAspectRatio);
            m_autoroiButtonLimits.yMax = ROI_AREA_HEIGHT;
            m_autoroiButtonLimits.xMin = (ROI_AREA_WIDTH - m_autoroiButtonLimits.xMax) / 2 ;
            m_autoroiButtonLimits.xMax += m_autoroiButtonLimits.xMin;
            m_autoroiButtonLimits.yMin = 0 ;
        }

        //
        // Step 3.
        //      We want to grab an image from the camera if we can.  However we can only grab
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
                            m_autoroiButtonLimits.xMax - m_autoroiButtonLimits.xMin,
                            m_autoroiButtonLimits.yMax - m_autoroiButtonLimits.yMin,
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
                        m_autoroiButtonLimits.xMax - m_autoroiButtonLimits.xMin,
                        m_autoroiButtonLimits.yMax - m_autoroiButtonLimits.yMin
                        , false, NULL);
                gtk_image_set_from_pixbuf (GTK_IMAGE (m_roiImage), m_roiBuf);
            }
        }
    }
}

// This will update the AUTOROI button so that it reflects the current AUTOROI.  Assumes m_autoroi and m_currentRoi are valid
void PxLAutoRoi::updateAutoroiButton()
{
    // The button should only be visible if we have a camera
    if (gCamera)
    {
        gtk_widget_show (m_autoroiButton);

        float widthPercent =   (float)m_autoroi.m_width / (float)m_currentRoi.m_width;
        float heightPercent =  (float)m_autoroi.m_height / (float)m_currentRoi.m_height;
        float offsetXPercent = (float)m_autoroi.m_offsetX / (float)m_currentRoi.m_width;
        float offsetYPercent = (float)m_autoroi.m_offsetY / (float)m_currentRoi.m_height;

        float xLimit = (float)(m_autoroiButtonLimits.xMax - m_autoroiButtonLimits.xMin);
        float yLimit = (float)(m_autoroiButtonLimits.yMax - m_autoroiButtonLimits.yMin);

        m_autoroiButtonRoi.m_width =  (int)(xLimit * widthPercent);
        m_autoroiButtonRoi.m_height = (int)(yLimit * heightPercent);
        m_autoroiButtonRoi.m_offsetX =  (int)(xLimit * offsetXPercent) + m_autoroiButtonLimits.xMin;
        m_autoroiButtonRoi.m_offsetY =  (int)(yLimit * offsetYPercent) + m_autoroiButtonLimits.yMin;
        setAutoroiButtonSize ();
        // Bugzilla.1304 -- This deprecated function is necessary because Ubuntu 14.04 on ARM uses an old library
        gtk_widget_set_margin_left (m_autoroiButton, m_autoroiButtonRoi.m_offsetX);
        gtk_widget_set_margin_top (m_autoroiButton, m_autoroiButtonRoi.m_offsetY);
    } else {
        gtk_widget_hide (m_autoroiButton);
    }
}

// Returns the operation type for a given mouse position
ROI_BUTTON_OPS PxLAutoRoi::determineOperationFromMouse (double relativeX, double relativeY)
{
    if (relativeY <= m_EdgeSensitivity)
    {
        if (relativeX <= m_EdgeSensitivity) return OP_RESIZE_TOP_LEFT;
        if (relativeX >= m_autoroiButtonRoi.m_width-m_EdgeSensitivity) return OP_RESIZE_TOP_RIGHT;
        return OP_RESIZE_TOP;
    } else if (relativeY >= m_autoroiButtonRoi.m_height-m_EdgeSensitivity) {
        if (relativeX <= m_EdgeSensitivity) return OP_RESIZE_BOTTOM_LEFT;
        if (relativeX >= m_autoroiButtonRoi.m_width-m_EdgeSensitivity) return OP_RESIZE_BOTTOM_RIGHT;
        return OP_RESIZE_BOTTOM;
    } else if (relativeX <= m_EdgeSensitivity) {
        return OP_RESIZE_LEFT;
    } else if (relativeX >= m_autoroiButtonRoi.m_width-m_EdgeSensitivity) {
        return OP_RESIZE_RIGHT;
    } else return OP_MOVE;

}

// Mouse down on the ROI button will start an operation
void PxLAutoRoi::startAutoroiButtonOperation( GtkWidget *widget, double relativeX, double relativeY, double absoluteX, double absoluteY)
{
    m_mouseXOnOpStart = absoluteX;
    m_mouseYOnOpStart = absoluteY;

    m_autoroiButtonOnOpStart = m_autoroiButtonRoi;

    m_currentOp = determineOperationFromMouse (relativeX, relativeY);

}

// Mouse up on the ROI button will finish an operation
void PxLAutoRoi::finishAutoroiButtonOperation( GtkWidget *widget)
{
    bool bPreviewRestoreRequired = false;
    bool bStreamRestoreRequired = false;

    //
    // Step 1
    //      Has the operation resulted in a new AUTOROI?  That will only be true if the user has moved
    //      mouse sufficiently while in a mouse down state
    if (abs (m_autoroiButtonRoi.m_width   - m_autoroiButtonOnOpStart.m_width)   >= m_EdgeSensitivity ||
        abs (m_autoroiButtonRoi.m_height  - m_autoroiButtonOnOpStart.m_height)  >= m_EdgeSensitivity ||
        abs (m_autoroiButtonRoi.m_offsetX - m_autoroiButtonOnOpStart.m_offsetX) >= m_EdgeSensitivity ||
        abs (m_autoroiButtonRoi.m_offsetY - m_autoroiButtonOnOpStart.m_offsetY) >= m_EdgeSensitivity)
    {
        //
        // Step 2
        //      Figure out what AUTOROI user wants based on the current size and position of the AUROI window
        float xLimit = (float)(m_autoroiButtonLimits.xMax - m_autoroiButtonLimits.xMin);
        float yLimit = (float)(m_autoroiButtonLimits.yMax - m_autoroiButtonLimits.yMin);

        float widthPercent =   (float)m_autoroiButtonRoi.m_width / xLimit;
        float heightPercent =  (float)m_autoroiButtonRoi.m_height / yLimit;
        float offsetXPercent = (float)(m_autoroiButtonRoi.m_offsetX - m_autoroiButtonLimits.xMin) / xLimit;
        float offsetYPercent = (float)(m_autoroiButtonRoi.m_offsetY - m_autoroiButtonLimits.yMin) / yLimit;

        //
        //  Step 3
        //      Rounding is going to occur because of the scaling we use for the AUTOROI (button) representation.  However,
        //      limit those rounding errors to just the dimensions that the user is adjusting
        PXL_ROI newAutoroi = m_autoroi;
        switch (m_currentOp)
        {
        case OP_MOVE:
            newAutoroi.m_offsetX =  (int)((float)m_currentRoi.m_width * offsetXPercent);
            newAutoroi.m_offsetY =  (int)((float)m_currentRoi.m_height * offsetYPercent);
            break;
        case OP_RESIZE_TOP_LEFT:
            newAutoroi.m_width =  (int)((float)m_currentRoi.m_width * widthPercent);
            newAutoroi.m_height = (int)((float)m_currentRoi.m_height * heightPercent);
            newAutoroi.m_offsetX =  (int)((float)m_currentRoi.m_width * offsetXPercent);
            newAutoroi.m_offsetY =  (int)((float)m_currentRoi.m_height * offsetYPercent);
            break;
        case OP_RESIZE_TOP_RIGHT:
            newAutoroi.m_width =  (int)((float)m_currentRoi.m_width * widthPercent);
            newAutoroi.m_height = (int)((float)m_currentRoi.m_height * heightPercent);
            newAutoroi.m_offsetX =  (int)((float)m_currentRoi.m_width * offsetXPercent);
            break;
        case OP_RESIZE_BOTTOM_RIGHT:
            newAutoroi.m_width =  (int)((float)m_currentRoi.m_width * widthPercent);
            newAutoroi.m_height = (int)((float)m_currentRoi.m_height * heightPercent);
            break;
        case OP_RESIZE_BOTTOM_LEFT:
            newAutoroi.m_width =  (int)((float)m_currentRoi.m_width * widthPercent);
            newAutoroi.m_height = (int)((float)m_currentRoi.m_height * heightPercent);
            newAutoroi.m_offsetX =  (int)((float)m_currentRoi.m_width * offsetXPercent);
            break;
        case OP_RESIZE_TOP:
            newAutoroi.m_height = (int)((float)m_currentRoi.m_height * heightPercent);
            newAutoroi.m_offsetY =  (int)((float)m_currentRoi.m_height * offsetYPercent);
            break;
        case OP_RESIZE_RIGHT:
            newAutoroi.m_width =  (int)((float)m_currentRoi.m_width * widthPercent);
            break;
        case OP_RESIZE_BOTTOM:
            newAutoroi.m_height = (int)((float)m_currentRoi.m_height * heightPercent);
            break;
        case OP_RESIZE_LEFT:
            newAutoroi.m_width =  (int)((float)m_currentRoi.m_width * widthPercent);
            newAutoroi.m_offsetX =  (int)((float)m_currentRoi.m_width * offsetXPercent);
            break;
        case OP_NONE:
        default:
            break;
        }

        //
        // Step 4
        //      Set this AUTOROI in the camera.  Note that the camera may make an adjustment to our AUTOROI

        PxLAutoLock lock(&gCameraLock);

        bool disabled = ! gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON(gAutoRoiTab->m_enable));

        bPreviewRestoreRequired = gCamera->previewing();
        bStreamRestoreRequired = gCamera->streaming() && ! gCamera->triggering();
        if (bPreviewRestoreRequired) gCamera->pausePreview();
        if (bStreamRestoreRequired)gCamera->stopStream();

        PXL_RETURN_CODE rc = gCamera->setRoiValue(AutoRoi, newAutoroi, disabled);
        if (API_SUCCESS(rc))
        {
            if (rc == ApiSuccessParametersChanged)
            {
                // The camera had to adjust the parameters to make them work, read it back to get the correct values.
                rc = gCamera->getRoiValue(AutoRoi, &newAutoroi);
            }
        }
        if (API_SUCCESS(rc))
        {
            m_autoroi = newAutoroi;
        }

        //
        // Step 5
        //      Update our controls to indicate we are using the new ROI

        // Step 5.1
        //      Update the ROI edits
        char cValue[40];

        sprintf (cValue, "%d",newAutoroi.m_offsetX);
        gtk_entry_set_text (GTK_ENTRY (m_offsetX), cValue);
        sprintf (cValue, "%d",newAutoroi.m_offsetY);
        gtk_entry_set_text (GTK_ENTRY (m_offsetY), cValue);
        sprintf (cValue, "%d",newAutoroi.m_width);
        gtk_entry_set_text (GTK_ENTRY (m_width), cValue);
        sprintf (cValue, "%d",newAutoroi.m_height);
        gtk_entry_set_text (GTK_ENTRY (m_height), cValue);

        //
        // Step 5.2
        //      Update the AUTOROI dropdown

        // Temporarily block the handler for ROI selection
        g_signal_handlers_block_by_func (gAutoRoiTab->m_autoroiCombo, (gpointer)NewAutoroiSelected, NULL);

        // If we are no longer using the 'custom' ROI at the end of our dropdown, remove it.
        if (m_usingNonstandardAutoroi)
        {
            gtk_combo_box_text_remove (GTK_COMBO_BOX_TEXT(gAutoRoiTab->m_autoroiCombo),
                                       --gAutoRoiTab->m_numAutoroisSupported);
            gAutoRoiTab->m_supportedAutorois.pop_back();
            gAutoRoiTab->m_usingNonstandardAutoroi = false;
        }

        int newSelectionIndex;
        std::vector<PXL_ROI>::iterator it;
        it = find (gAutoRoiTab->m_supportedAutorois.begin(), gAutoRoiTab->m_supportedAutorois.end(), newAutoroi);
        if (it == gAutoRoiTab->m_supportedAutorois.end())
        {
            // This is a new 'non standard' ROI.  Add it to the end
            newSelectionIndex = gAutoRoiTab->m_numAutoroisSupported;
            sprintf (cValue, "%d x %d", newAutoroi.m_width, newAutoroi.m_height);
            gtk_combo_box_text_insert_text (GTK_COMBO_BOX_TEXT(gAutoRoiTab->m_autoroiCombo),
                                            gAutoRoiTab->m_numAutoroisSupported++,
                                            cValue);
            gAutoRoiTab->m_supportedAutorois.push_back(newAutoroi);
            gAutoRoiTab->m_usingNonstandardAutoroi = true;
        } else {
            // the user 'created' a standard ROI
            newSelectionIndex = it - gAutoRoiTab->m_supportedAutorois.begin();
        }
        gtk_combo_box_set_active (GTK_COMBO_BOX(gAutoRoiTab->m_autoroiCombo),newSelectionIndex);
        // Unblock the handler for AUTOROI selection
        g_signal_handlers_unblock_by_func (gAutoRoiTab->m_autoroiCombo, (gpointer)NewAutoroiSelected, NULL);
    }

    // Reflect these adjustments in the ROI button
    loadRoiImage (bStreamRestoreRequired);
    updateAutoroiButton();
    if (bStreamRestoreRequired) gCamera->startStream();
    if (bPreviewRestoreRequired) gCamera->playPreview();

    m_currentOp = OP_NONE;
}

// Set the size of the AUTOROI button, including putting in the label if there is room
void PxLAutoRoi::setAutoroiButtonSize ()
{
    bool bLabel;  // true if we want the ROI to have a label

     bLabel = (m_autoroiButtonRoi.m_width >= AUTOROI_BUTTON_LABEL_THRESHOLD_WIDTH) &&
              (m_autoroiButtonRoi.m_height >= AUTOROI_BUTTON_LABEL_THRESHOLD_HEIGHT);
     gtk_button_set_label (GTK_BUTTON(m_autoroiButton), bLabel ? AutoroiLabel: "");
     gtk_widget_set_size_request (m_autoroiButton, m_autoroiButtonRoi.m_width, m_autoroiButtonRoi.m_height);
}

/* ---------------------------------------------------------------------------
 * --   gtk thread callbacks - used to update controls
 * ---------------------------------------------------------------------------
 */

// Indicate that the refresh is no longer outstanding, it has completed.
static gboolean RefreshComplete (gpointer pData)
{
    PxLAutoRoi *pAutoRoi = (PxLAutoRoi *)pData;

    pAutoRoi->m_numRefreshRequestsOutstanding--;
    return false;
}

//
// Make ROI meaningless
static gboolean AutoroiDeactivate (gpointer pData)
{
    PxLAutoRoi *pAutoRoi = (PxLAutoRoi *)pData;

    gtk_widget_set_sensitive (pAutoRoi->m_enable, false);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(pAutoRoi->m_enable), false);
    gtk_widget_set_sensitive (pAutoRoi->m_autoroiCombo, false);
    gtk_widget_set_sensitive (pAutoRoi->m_offsetX, false);
    gtk_widget_set_sensitive (pAutoRoi->m_offsetY, false);
    gtk_widget_set_sensitive (pAutoRoi->m_width, false);
    gtk_widget_set_sensitive (pAutoRoi->m_height, false);
    gtk_widget_set_sensitive (pAutoRoi->m_center, false);

    pAutoRoi->m_roiBuf = NULL; // Don't use the current FFOV image

    pAutoRoi->loadRoiImage();
    pAutoRoi->updateAutoroiButton();

    return false;  //  Only run once....
}

//
// Assert all of the ROI controls
static gboolean AutoroiActivate (gpointer pData)
{
    PxLAutoRoi *pAutoRoi = (PxLAutoRoi *)pData;

    bool settable = false;
    bool enabled  = false;

    //
    // Step 0
    //      Read the default cursor.
    pAutoRoi->m_originalCursor = gdk_window_get_cursor (gtk_widget_get_window (pAutoRoi->m_roiImage));

    PxLAutoLock lock(&gCameraLock);

    if (gCamera)
    {
        pAutoRoi->m_numAutoroisSupported = 0;
        pAutoRoi->m_supportedAutorois.clear();
        pAutoRoi->m_usingNonstandardAutoroi = false;
        gtk_combo_box_text_remove_all (GTK_COMBO_BOX_TEXT(pAutoRoi->m_autoroiCombo));

        if (gCamera->supportedInCamera(FEATURE_AUTO_ROI))
        {
            settable = gCamera->settable (FEATURE_AUTO_ROI);
            enabled = gCamera->enabled (FEATURE_AUTO_ROI);

            PXL_ROI minAutoroi, maxAutoroi, currentAutoroi, currentRoi;
            PXL_RETURN_CODE rc  = gCamera->getRoiValue (AutoRoi, &currentAutoroi);
            PXL_RETURN_CODE rc2 = gCamera->getRoiValue (FrameRoi, &currentRoi);

            if (API_SUCCESS(rc && rc2))
            {
                char cValue[40];

                //
                // Step 1
                //      ROI Size and location edits
                sprintf (cValue, "%d",currentAutoroi.m_offsetX);
                gtk_entry_set_text (GTK_ENTRY (pAutoRoi->m_offsetX), cValue);
                sprintf (cValue, "%d",currentAutoroi.m_offsetY);
                gtk_entry_set_text (GTK_ENTRY (pAutoRoi->m_offsetY), cValue);
                sprintf (cValue, "%d",currentAutoroi.m_width);
                gtk_entry_set_text (GTK_ENTRY (pAutoRoi->m_width), cValue);
                sprintf (cValue, "%d",currentAutoroi.m_height);
                gtk_entry_set_text (GTK_ENTRY (pAutoRoi->m_height), cValue);

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
                    //      Add all of the standard AUTOROIS that are larger than min, and smaller than max, and are supported
                    //      by the camera
                    rc = gCamera->getRoiRange (AutoRoi, &minAutoroi, &maxAutoroi, false);
                    maxAutoroi.m_offsetX = maxAutoroi.m_offsetY = 0;  // Offests are 0 for maxAutoroi
                    if (API_SUCCESS(rc))
                    {
                        // record the maxRoi, to avoid having to read it a bunch of times.
                        pAutoRoi->m_maxAutoroi   = maxAutoroi;
                        pAutoRoi->m_currentRoi = currentRoi;
                        pAutoRoi->m_roiAspectRatio = (float)currentRoi.m_width / (float)currentRoi.m_height;

                        // Step 2.3
                        //      If we are rotating 90 or 270 degrees, we need to flip the 'standard' ROIs
                        bool  transpose = false;
                        float rotation = 0.0;
                        rc = gCamera->getValue (FEATURE_ROTATE, &rotation);
                        if (API_SUCCESS (rc) && (rotation == 90.0 || rotation == 270.0)) transpose = true;
                        for (int i=0; i < (int)PxLStandardRois.size(); i++)
                        {
                            if (PxLStandardRois[i] < minAutoroi) continue;
                            if (PxLStandardRois[i] > maxAutoroi) break;
                            if (PxLStandardRois[i] > currentRoi) break;
                            //
                            // 2018-01-25
                            //     As an 'optimzation, don't bother checking each of the indivdual ROIs, simply add
                            //     each of the standard ones (that will fit).  This menas that the times in the list
                            //     might not be supported EXACTLY in the camera; the camera may need to adjust them
                            //     to make it fit.
                            //rc = gCamera->setRoiValue (AutoRoi, PxLStandardRois[i]);
                            if (API_SUCCESS(rc))
                            {
                                //if (PxLStandardRois[i] != currentAutoroi) restoreRequired = true;
                                PXL_ROI adjustedAutoroi = PxLStandardRois[i];
                                if (transpose) adjustedAutoroi.rotateClockwise(rotation);
                                //if (rc == ApiSuccessParametersChanged)
                                //{
                                //    rc = gCamera->getRoiValue (AutoRoi, &adjustedRoi);
                                //}
                                if (API_SUCCESS(rc))
                                {
                                    sprintf (cValue, "%d x %d", adjustedAutoroi.m_width, adjustedAutoroi.m_height);
                                    gtk_combo_box_text_insert_text (GTK_COMBO_BOX_TEXT(pAutoRoi->m_autoroiCombo),
                                                                    pAutoRoi->m_numAutoroisSupported++,
                                                                    cValue);
                                    pAutoRoi->m_supportedAutorois.push_back(adjustedAutoroi);
                                }
                            }
                        }

                        // Step 2.3
                        //      If it's not already there, add the max ROI to the end of the list
                        if (pAutoRoi->m_numAutoroisSupported == 0 || pAutoRoi->m_supportedAutorois[pAutoRoi->m_numAutoroisSupported-1] != maxAutoroi)
                        {
                            sprintf (cValue, "%d x %d", maxAutoroi.m_width, maxAutoroi.m_height);
                            gtk_combo_box_text_insert_text (GTK_COMBO_BOX_TEXT(pAutoRoi->m_autoroiCombo),
                                                            pAutoRoi->m_numAutoroisSupported++,
                                                            cValue);
                            pAutoRoi->m_supportedAutorois.push_back(maxAutoroi);
                        }

                        // Step 2.4
                        //      If the current ROI isn't one of the standard one from the list, add this 'custom' one
                        //      to the end.
                        if (find (pAutoRoi->m_supportedAutorois.begin(),
                                  pAutoRoi->m_supportedAutorois.end(),
                                  currentAutoroi) == pAutoRoi->m_supportedAutorois.end())
                        {
                            sprintf (cValue, "%d x %d", currentAutoroi.m_width, currentAutoroi.m_height);
                            gtk_combo_box_text_insert_text (GTK_COMBO_BOX_TEXT(pAutoRoi->m_autoroiCombo),
                                                            pAutoRoi->m_numAutoroisSupported++,
                                                            cValue);
                            pAutoRoi->m_supportedAutorois.push_back(currentAutoroi);
                            pAutoRoi->m_usingNonstandardAutoroi = true;
                        }

                        // Step 2.5
                        //      If we changed the ROI, restore the original one
                        if (restoreRequired)
                        {
                            rc = gCamera->setRoiValue (AutoRoi, currentAutoroi);
                        }
                    }

                    // Step 3
                    //      Mark the current ROI as 'selected'
                    for (int i=0; i<(int)pAutoRoi->m_supportedAutorois.size(); i++)
                    {
                        if (pAutoRoi->m_supportedAutorois[i] != currentAutoroi) continue;
                        gtk_combo_box_set_active (GTK_COMBO_BOX(pAutoRoi->m_autoroiCombo),i);
                        break;
                    }

                    // Step 4
                    //      Remember the current ROI
                    pAutoRoi->m_autoroi = currentAutoroi;

                }
            }
        }
    }

    // Update the FFOV image and ROI button
    pAutoRoi->loadRoiImage();
    pAutoRoi->updateAutoroiButton ();

    // ROI controls
    gtk_widget_set_sensitive (pAutoRoi->m_offsetX, settable);
    gtk_widget_set_sensitive (pAutoRoi->m_offsetY, settable);
    gtk_widget_set_sensitive (pAutoRoi->m_width, settable);
    gtk_widget_set_sensitive (pAutoRoi->m_height, settable);

    gtk_widget_set_sensitive (pAutoRoi->m_center, settable);

    //ROI drop-down
    gtk_widget_set_sensitive (pAutoRoi->m_autoroiCombo, settable);

    // AUTO-ROI Enable
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(pAutoRoi->m_enable), settable && enabled);

    return false;  //  Only run once....
}

//
// Make focus controls meaningless
static gboolean OperationsDeactivate (gpointer pData)
{
    PxLAutoRoi *pAutoRoi = (PxLAutoRoi *)pData;

    gtk_widget_set_sensitive (pAutoRoi->m_exposure, false);
    gtk_widget_set_sensitive (pAutoRoi->m_gain, false);
    gtk_widget_set_sensitive (pAutoRoi->m_red, false);
    gtk_widget_set_sensitive (pAutoRoi->m_green, false);
    gtk_widget_set_sensitive (pAutoRoi->m_blue, false);

    gtk_widget_set_sensitive (pAutoRoi->m_exposureOneTime, false);
    gtk_widget_set_sensitive (pAutoRoi->m_gainOneTime, false);
    gtk_widget_set_sensitive (pAutoRoi->m_whiteBalanceOneTime, false);

    if (gCamera)
    {
        gCamera->m_poller->pollRemove(exposureFuncs);
        gCamera->m_poller->pollRemove(gainFuncs);
    }

    return false;  //  Only run once....
}

//
// Assert all of the focus controls
static gboolean OperationsActivate (gpointer pData)
{
    PxLAutoRoi *pAutoRoi = (PxLAutoRoi *)pData;

    float values[3];
    char cValue[40];
    bool oneTimeExposureSupported = false;
    bool oneTimeGainSupported = false;
    bool oneTimeWhiteBalanceSupported = false;
    bool continuousExposureCurrentlyOn = false;
    bool continuousGainCurrentlyOn = false;
    bool continuousWhiteBalanceCurrentlyOn = false;

    PxLAutoLock lock(&gCameraLock);

    if (gCamera)
    {
        if (gCamera->supported(FEATURE_EXPOSURE))
        {
            if (API_SUCCESS (gCamera->getValue(FEATURE_EXPOSURE, values)))
            {
                sprintf (cValue, "%5.2f",values[0]*1000.0f);
                gtk_entry_set_text (GTK_ENTRY (pAutoRoi->m_exposure), cValue);
            }

            oneTimeExposureSupported = gCamera->oneTimeSuppored(FEATURE_EXPOSURE);
            gCamera->getContinuousAuto(FEATURE_EXPOSURE, &continuousExposureCurrentlyOn);
        }

        if (gCamera->supported(FEATURE_GAIN))
        {
            if (API_SUCCESS (gCamera->getValue(FEATURE_GAIN, values)))
            {
                sprintf (cValue, "%5.2f",values[0]);
                gtk_entry_set_text (GTK_ENTRY (pAutoRoi->m_gain), cValue);
            }

            oneTimeGainSupported = gCamera->oneTimeSuppored(FEATURE_GAIN);
            gCamera->getContinuousAuto(FEATURE_GAIN, &continuousGainCurrentlyOn);
        }

        if (gCamera->supported(FEATURE_WHITE_SHADING))
        {
            if (API_SUCCESS (gCamera->getWhiteBalanceValues(&values[0], &values[1], &values[2])))
            {
                sprintf (cValue, "%5.2f",values[0]);
                gtk_entry_set_text (GTK_ENTRY (pAutoRoi->m_red), cValue);
                sprintf (cValue, "%5.2f",values[1]);
                gtk_entry_set_text (GTK_ENTRY (pAutoRoi->m_green), cValue);
                sprintf (cValue, "%5.2f",values[2]);
                gtk_entry_set_text (GTK_ENTRY (pAutoRoi->m_blue), cValue);
            }

            oneTimeWhiteBalanceSupported = gCamera->oneTimeSuppored(FEATURE_WHITE_SHADING);
            gCamera->getContinuousAuto(FEATURE_WHITE_SHADING, &continuousWhiteBalanceCurrentlyOn);
        }
    }

    gtk_widget_set_sensitive (pAutoRoi->m_exposureOneTime,
            oneTimeExposureSupported && !continuousExposureCurrentlyOn);
    gtk_widget_set_sensitive (pAutoRoi->m_gainOneTime,
            oneTimeGainSupported && !continuousGainCurrentlyOn);
    gtk_widget_set_sensitive (pAutoRoi->m_whiteBalanceOneTime,
            oneTimeWhiteBalanceSupported && !continuousWhiteBalanceCurrentlyOn);

    // add our functions to the continuous poller
    if (continuousExposureCurrentlyOn) gCamera->m_poller->pollAdd(exposureFuncs);
    if (continuousGainCurrentlyOn) gCamera->m_poller->pollAdd(gainFuncs);

    return false;  //  Only run once....
}

//
// Called periodically when doing a one-time exposure updates -- reads the current value
static PXL_RETURN_CODE GetCurrentExposure()
{
    PXL_RETURN_CODE rc = ApiSuccess;

    PxLAutoLock lock(&gCameraLock);
    if (gCamera && gAutoRoiTab)
    {
        // It's safe to assume the camera supports exposure, as this function will not be called
        // otherwise.  If we were to check via pCamera->supported (FEATURE_EXPOSURE) or
        // pCamera->continuousSupported (FEATURE_EXPSOURE), then that will perform a PxLGetCameraFeatures,
        // which is a lot of work for not.
        float exposureinSeconds = 0.0;
        rc = gCamera->getValue(FEATURE_EXPOSURE, &exposureinSeconds);
        if (API_SUCCESS(rc)) gAutoRoiTab->m_exposureLast = exposureinSeconds * 1000.0f;
    }

    return rc;
}

//
// Called periodically when doing one-time exposure updates -- updates the user controls
static void UpdateExposureControls()
{
    char cValue[40];

    if (gCamera && gAutoRoiTab)
    {
        PxLAutoLock lock(&gCameraLock);

        sprintf (cValue, "%5.2f",gAutoRoiTab->m_exposureLast);
        gtk_entry_set_text (GTK_ENTRY (gAutoRoiTab->m_exposure), cValue);

        bool onetimeExposureOn = false;
        bool continuousExposureOn = false;
        if (gCamera->m_poller->polling (exposureFuncs))
        {
            gCamera->getOnetimeAuto (FEATURE_EXPOSURE, &onetimeExposureOn);
            gCamera->getContinuousAuto(FEATURE_EXPOSURE, &continuousExposureOn);
        }
        if (!onetimeExposureOn && !continuousExposureOn)
        {
            // No need to poll any longer
            gCamera->m_poller->pollRemove(exposureFuncs);
            // Reflect these adjustments in the ROI button
            gAutoRoiTab->loadRoiImage ();
            gAutoRoiTab->updateAutoroiButton();
        }
    }
}

//
// Called periodically when doing a one-time gain updates -- reads the current value
static PXL_RETURN_CODE GetCurrentGain()
{
    PXL_RETURN_CODE rc = ApiSuccess;

    PxLAutoLock lock(&gCameraLock);
    if (gCamera && gAutoRoiTab)
    {
        // It's safe to assume the camera supports gain, as this function will not be called
        // otherwise.  If we were to check via pCamera->supported (FEATURE_GAIN) or
        // pCamera->continuousSupported (FEATURE_GAIN), then that will perform a PxLGetCameraFeatures,
        // which is a lot of work for not.
        float gain = 0.0;
        rc = gCamera->getValue(FEATURE_GAIN, &gain);
        if (API_SUCCESS(rc)) gAutoRoiTab->m_gainLast = gain;
    }

    return rc;
}

//
// Called periodically when doing one-time gain updates -- updates the user controls
static void UpdateGainControls()
{
    char cValue[40];

    if (gCamera && gAutoRoiTab)
    {
        PxLAutoLock lock(&gCameraLock);

        sprintf (cValue, "%5.2f",gAutoRoiTab->m_gainLast);
        gtk_entry_set_text (GTK_ENTRY (gAutoRoiTab->m_gain), cValue);

        bool onetimeOn = false;
        bool continuousOn = false;
        if (gCamera->m_poller->polling (gainFuncs))
        {
            gCamera->getOnetimeAuto (FEATURE_GAIN, &onetimeOn);
            gCamera->getContinuousAuto(FEATURE_GAIN, &continuousOn);
        }
        if (!onetimeOn && !continuousOn)
        {
            // No need to poll any longer
            gCamera->m_poller->pollRemove(gainFuncs);
            // Reflect these adjustments in the ROI button
            gAutoRoiTab->loadRoiImage ();
            gAutoRoiTab->updateAutoroiButton();
        }
    }
}

//
// Called periodically when doing a one-time White Balance updates -- reads the current value
static PXL_RETURN_CODE GetCurrentWhitebalance()
{
    PXL_RETURN_CODE rc = ApiSuccess;

    PxLAutoLock lock(&gCameraLock);
    if (gCamera && gAutoRoiTab)
    {
        // It's safe to assume the camera supports white balance, as this function will not be called
        // otherwise.  If we were to check via pCamera->supported (FEATURE_WHITE_BALANCE) or
        // pCamera->continuousSupported (FEATURE_WHITE_BALANCE), then that will perform a PxLGetCameraFeatures,
        // which is a lot of work for not.
        float currentRed;
        float currentGreen;
        float currentBlue;
        rc = gCamera->getWhiteBalanceValues(&currentRed, &currentGreen, &currentBlue);
        if (API_SUCCESS(rc))
        {
            gAutoRoiTab->m_redLast = currentRed;
            gAutoRoiTab->m_greenLast = currentGreen;
            gAutoRoiTab->m_blueLast = currentBlue;
        }
    }

    return rc;
}

//
// Called periodically when doing one-time white balance updates -- updates the user controls
static void UpdateWhitebalanceControls()
{
    char cValue[40];

    if (gCamera && gAutoRoiTab)
    {
        PxLAutoLock lock(&gCameraLock);

        sprintf (cValue, "%5.2f",gAutoRoiTab->m_redLast);
        gtk_entry_set_text (GTK_ENTRY (gAutoRoiTab->m_red), cValue);
        sprintf (cValue, "%5.2f",gAutoRoiTab->m_greenLast);
        gtk_entry_set_text (GTK_ENTRY (gAutoRoiTab->m_green), cValue);
        sprintf (cValue, "%5.2f",gAutoRoiTab->m_blueLast);
        gtk_entry_set_text (GTK_ENTRY (gAutoRoiTab->m_blue), cValue);

        bool onetimeOn = false;
        if (gCamera->m_poller->polling (whitebalanceFuncs))
        {
            gCamera->getOnetimeAuto (FEATURE_WHITE_SHADING, &onetimeOn);
        }
        if (!onetimeOn)
        {
            // No need to poll any longer
            gCamera->m_poller->pollRemove(whitebalanceFuncs);
            // Reflect these adjustments in the ROI button
            gAutoRoiTab->loadRoiImage ();
            gAutoRoiTab->updateAutoroiButton();
        }
    }
}

/* ---------------------------------------------------------------------------
 * --   Control functions from the Glade project
 * ---------------------------------------------------------------------------
 */

extern "C" void NewAutoroiSelected
    (GtkWidget* widget, GdkEventExpose* event, gpointer userdata )
{
    if (! gCamera || ! gCamera) return;
    if (gAutoRoiTab->m_numRefreshRequestsOutstanding) return;

    bool disabled = ! gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON(gAutoRoiTab->m_enable));
    PxLAutoLock lock(&gCameraLock);

    int roiIndex = gtk_combo_box_get_active (GTK_COMBO_BOX(gAutoRoiTab->m_autoroiCombo));
    // This routine may have been triggered by the user changing the custom ROI value, in
    // which case it is gone.  That instance is handled completely by the edit field handler
    if (roiIndex < 0 || roiIndex >= (int)gAutoRoiTab->m_supportedAutorois.size()) return;

    PXL_ROI newAutoroi = gAutoRoiTab->m_supportedAutorois[roiIndex];

    // Try to keep the new AUTOROI centered on the old AUTOROI.  We need to read the AUTOROI limits
    // to determine this.
    PXL_RETURN_CODE rc;

    int oldCenterX = gAutoRoiTab->m_autoroi.m_offsetX + (gAutoRoiTab->m_autoroi.m_width/2);
    int oldCenterY = gAutoRoiTab->m_autoroi.m_offsetY + (gAutoRoiTab->m_autoroi.m_height/2);
    int newOffsetX = oldCenterX - newAutoroi.m_width/2;
    int newOffsetY = oldCenterY - newAutoroi.m_height/2;

    // However, we may need to move our center if the new offset does not fit
    if (newOffsetX < 0) newOffsetX = 0;
    if (newOffsetY < 0) newOffsetY = 0;
    if (newOffsetX + newAutoroi.m_width > gAutoRoiTab->m_currentRoi.m_width) newOffsetX = gAutoRoiTab->m_currentRoi.m_width - newAutoroi.m_width;
    if (newOffsetY + newAutoroi.m_height > gAutoRoiTab->m_currentRoi.m_height) newOffsetY = gAutoRoiTab->m_currentRoi.m_height - newAutoroi.m_height;

    newAutoroi.m_offsetX = newOffsetX;
    newAutoroi.m_offsetY = newOffsetY;

    rc = gCamera->setRoiValue(AutoRoi, newAutoroi, disabled);
    if (API_SUCCESS(rc))
    {
        if (rc == ApiSuccessParametersChanged)
        {
            // The camera had to adjust the parameters to make them work, read it back to get the correct values.
            rc = gCamera->getRoiValue(AutoRoi, &newAutoroi);
        }
    }
    if (API_SUCCESS(rc))
    {
        gAutoRoiTab->m_autoroi = newAutoroi;

        // Update our controls to indicate we are using the new ROI
        char cValue[40];

        sprintf (cValue, "%d",newAutoroi.m_offsetX);
        gtk_entry_set_text (GTK_ENTRY (gAutoRoiTab->m_offsetX), cValue);
        sprintf (cValue, "%d",newAutoroi.m_offsetY);
        gtk_entry_set_text (GTK_ENTRY (gAutoRoiTab->m_offsetY), cValue);
        sprintf (cValue, "%d",newAutoroi.m_width);
        gtk_entry_set_text (GTK_ENTRY (gAutoRoiTab->m_width), cValue);
        sprintf (cValue, "%d",newAutoroi.m_height);
        gtk_entry_set_text (GTK_ENTRY (gAutoRoiTab->m_height), cValue);

        // If we are no longer using the 'custom' ROI at the end of our dropdown, remove it.
        if (gAutoRoiTab->m_usingNonstandardAutoroi && roiIndex != gAutoRoiTab->m_numAutoroisSupported-1)
        {
            gtk_combo_box_text_remove (GTK_COMBO_BOX_TEXT(gAutoRoiTab->m_autoroiCombo),
                                       --gAutoRoiTab->m_numAutoroisSupported);
            gAutoRoiTab->m_supportedAutorois.pop_back();
            gAutoRoiTab->m_usingNonstandardAutoroi = false;
        }

        // Update the AUTOROI button
        gAutoRoiTab->loadRoiImage();
        gAutoRoiTab->updateAutoroiButton();
    }
}

extern "C" void AutoroiOffsetXValueChanged
    (GtkWidget* widget, GdkEventExpose* event, gpointer userdata )
{
    if (! gCamera || ! gAutoRoiTab) return;
    if (gAutoRoiTab->m_numRefreshRequestsOutstanding) return;

    bool disabled = ! gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON(gAutoRoiTab->m_enable));

    PxLAutoLock lock(&gCameraLock);

    int newOffsetX = atoi (gtk_entry_get_text (GTK_ENTRY (gAutoRoiTab->m_offsetX)));
    PXL_RETURN_CODE rc;

    PXL_ROI newAutoroi = gAutoRoiTab->m_autoroi;

    if (newOffsetX + newAutoroi.m_width > gAutoRoiTab->m_currentRoi.m_width) newOffsetX = gAutoRoiTab->m_currentRoi.m_width - newAutoroi.m_width;
    newAutoroi.m_offsetX = newOffsetX;

    rc = gCamera->setRoiValue(AutoRoi, newAutoroi, disabled);
    if (API_SUCCESS (rc))
    {
        // Read it back again, just in case the camera did some 'tuning' of the values
        rc = gCamera->getRoiValue(AutoRoi, &newAutoroi);

        if (API_SUCCESS (rc))
        {
            gAutoRoiTab->m_autoroi = newAutoroi;
        }

        // Update the ROI button
        gAutoRoiTab->loadRoiImage();
        gAutoRoiTab->updateAutoroiButton();
    }

    // Reassert the current value even if we did not succeed in setting it
    char cValue[40];
    sprintf (cValue, "%d",gAutoRoiTab->m_autoroi.m_offsetX);
    gtk_entry_set_text (GTK_ENTRY (gAutoRoiTab->m_offsetX), cValue);
}

extern "C" void AutoroiOffsetYValueChanged
    (GtkWidget* widget, GdkEventExpose* event, gpointer userdata )
{
    if (! gCamera || ! gAutoRoiTab) return;
    if (gAutoRoiTab->m_numRefreshRequestsOutstanding) return;

    bool disabled = ! gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON(gAutoRoiTab->m_enable));

    PxLAutoLock lock(&gCameraLock);

    int newOffsetY = atoi (gtk_entry_get_text (GTK_ENTRY (gAutoRoiTab->m_offsetY)));
    PXL_RETURN_CODE rc;

    PXL_ROI newAutoroi = gAutoRoiTab->m_autoroi;

    if (newOffsetY + newAutoroi.m_height > gAutoRoiTab->m_currentRoi.m_height) newOffsetY = gAutoRoiTab->m_currentRoi.m_height - newAutoroi.m_height;
    newAutoroi.m_offsetY = newOffsetY;

    rc = gCamera->setRoiValue(AutoRoi, newAutoroi, disabled);
    if (API_SUCCESS (rc))
    {
        // Read it back again, just in case the camera did some 'tuning' of the values
        rc = gCamera->getRoiValue(AutoRoi, &newAutoroi);

        if (API_SUCCESS (rc))
        {
            gAutoRoiTab->m_autoroi = newAutoroi;
        }

        // Update the ROI button
        gAutoRoiTab->loadRoiImage();
        gAutoRoiTab->updateAutoroiButton();
    }

    // Reassert the current value even if we did not succeed in setting it
    char cValue[40];
    sprintf (cValue, "%d",gAutoRoiTab->m_autoroi.m_offsetY);
    gtk_entry_set_text (GTK_ENTRY (gAutoRoiTab->m_offsetY), cValue);
}

extern "C" void AutoroiWidthValueChanged
    (GtkWidget* widget, GdkEventExpose* event, gpointer userdata )
{
    if (! gCamera || ! gAutoRoiTab) return;
    if (gAutoRoiTab->m_numRefreshRequestsOutstanding) return;

    bool disabled = ! gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON(gAutoRoiTab->m_enable));

    PxLAutoLock lock(&gCameraLock);

    char cValue[40];

    // NOTE, this procedure will adjust the offsetX value, if doing so will accommodate the user
    // suppled width
    int newWidth = atoi (gtk_entry_get_text (GTK_ENTRY (gAutoRoiTab->m_width)));
    int newOffsetX = gAutoRoiTab->m_autoroi.m_offsetX;

    PXL_RETURN_CODE rc;

    PXL_ROI newAutoroi = gAutoRoiTab->m_autoroi;

    // Trim the width if it is too big
    newWidth = min (newWidth, gAutoRoiTab->m_maxAutoroi.m_width);

    // Trim the offset if we need to to accomodate the width
    if (newWidth + newAutoroi.m_offsetX > gAutoRoiTab->m_currentRoi.m_width)
    {
        newOffsetX = gAutoRoiTab->m_currentRoi.m_width - newWidth;
    }
    if (newOffsetX >= 0)
    {
        // Looks like we found a width and offsetX that should work
        newAutoroi.m_width = newWidth;
        newAutoroi.m_offsetX = newOffsetX;

        rc = gCamera->setRoiValue(AutoRoi, newAutoroi, disabled);
        if (API_SUCCESS (rc))
        {
            // Read it back again, just in case the camera did some 'tuning' of the values
            rc = gCamera->getRoiValue(AutoRoi, &newAutoroi);
            if (API_SUCCESS (rc))
            {
                gAutoRoiTab->m_autoroi = newAutoroi;

                // Update the controls with the new ROI width (and offsetx, as it may have changed.  Note that
                // the actual edit field is done later, even on failure.
                sprintf (cValue, "%d",gAutoRoiTab->m_autoroi.m_offsetX);
                gtk_entry_set_text (GTK_ENTRY (gAutoRoiTab->m_offsetX), cValue);
                if (gAutoRoiTab->m_usingNonstandardAutoroi)
                {
                    gtk_combo_box_text_remove (GTK_COMBO_BOX_TEXT(gAutoRoiTab->m_autoroiCombo),
                                               --gAutoRoiTab->m_numAutoroisSupported);
                    gAutoRoiTab->m_supportedAutorois.pop_back();
                    gAutoRoiTab->m_usingNonstandardAutoroi = false;
                }

                int newSelectionIndex;
                std::vector<PXL_ROI>::iterator it;
                it = find (gAutoRoiTab->m_supportedAutorois.begin(), gAutoRoiTab->m_supportedAutorois.end(), newAutoroi);
                if (it == gAutoRoiTab->m_supportedAutorois.end())
                {
                    // This is a new 'non standard' ROI.  Add it to the end
                    newSelectionIndex = gAutoRoiTab->m_numAutoroisSupported;
                    sprintf (cValue, "%d x %d", newAutoroi.m_width, newAutoroi.m_height);
                    gtk_combo_box_text_insert_text (GTK_COMBO_BOX_TEXT(gAutoRoiTab->m_autoroiCombo),
                                                    gAutoRoiTab->m_numAutoroisSupported++,
                                                    cValue);
                    gAutoRoiTab->m_supportedAutorois.push_back(newAutoroi);
                    gAutoRoiTab->m_usingNonstandardAutoroi = true;
                } else {
                    // the user 'created' a standard ROI
                    newSelectionIndex = it - gAutoRoiTab->m_supportedAutorois.begin();
                }
                gtk_combo_box_set_active (GTK_COMBO_BOX(gAutoRoiTab->m_autoroiCombo),newSelectionIndex);

            }

            // Update the ROI button
            gAutoRoiTab->loadRoiImage();
            gAutoRoiTab->updateAutoroiButton();
        }
    }

    // Reassert the current value even if we did not succeed in setting it
    sprintf (cValue, "%d",gAutoRoiTab->m_autoroi.m_width);
    gtk_entry_set_text (GTK_ENTRY (gAutoRoiTab->m_width), cValue);
}

extern "C" void AutoroiHeightValueChanged
    (GtkWidget* widget, GdkEventExpose* event, gpointer userdata )
{
    if (! gCamera || ! gAutoRoiTab) return;
    if (gAutoRoiTab->m_numRefreshRequestsOutstanding) return;

    bool disabled = ! gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON(gAutoRoiTab->m_enable));

    PxLAutoLock lock(&gCameraLock);

    char cValue[40];

    // NOTE, this procedure will adjust the offsetY value, if doing so will accommodate the user
    // suppled height
    int newHeight = atoi (gtk_entry_get_text (GTK_ENTRY (gAutoRoiTab->m_height)));
    int newOffsetY = gAutoRoiTab->m_autoroi.m_offsetY;

    PXL_RETURN_CODE rc;
    PXL_ROI newAutoroi = gAutoRoiTab->m_autoroi;

    // trim the height if needed
    newHeight = min (newHeight, gAutoRoiTab->m_maxAutoroi.m_height);
    if (newHeight + newAutoroi.m_offsetY > gAutoRoiTab->m_currentRoi.m_height)
    {
        newOffsetY = gAutoRoiTab->m_currentRoi.m_height - newHeight;
    }
    if (newOffsetY >= 0)
    {
        // Looks like we found a width and offsetX that should work
        newAutoroi.m_height = newHeight;
        newAutoroi.m_offsetY = newOffsetY;

        rc = gCamera->setRoiValue(AutoRoi, newAutoroi, disabled);
        if (API_SUCCESS (rc))
        {
            // Read it back again, just in case the camera did some 'tuning' of the values
            rc = gCamera->getRoiValue(AutoRoi, &newAutoroi);

            if (API_SUCCESS (rc))
            {
                gAutoRoiTab->m_autoroi = newAutoroi;

                // Update the controls with the new ROI height (and offsety, as it may have changed.  Note that
                // the actual edit field is done later, even on failure.
                sprintf (cValue, "%d",gAutoRoiTab->m_autoroi.m_offsetY);
                gtk_entry_set_text (GTK_ENTRY (gAutoRoiTab->m_offsetY), cValue);
                if (gAutoRoiTab->m_usingNonstandardAutoroi)
                {
                    gtk_combo_box_text_remove (GTK_COMBO_BOX_TEXT(gAutoRoiTab->m_autoroiCombo),
                                               --gAutoRoiTab->m_numAutoroisSupported);
                    gAutoRoiTab->m_supportedAutorois.pop_back();
                    gAutoRoiTab->m_usingNonstandardAutoroi = false;
                }
                int newSelectionIndex;
                std::vector<PXL_ROI>::iterator it;
                it = find (gAutoRoiTab->m_supportedAutorois.begin(), gAutoRoiTab->m_supportedAutorois.end(), newAutoroi);
                if (it == gAutoRoiTab->m_supportedAutorois.end())
                {
                    // This is a new 'non standard' ROI.  Add it to the end
                    newSelectionIndex = gAutoRoiTab->m_numAutoroisSupported;
                    sprintf (cValue, "%d x %d", newAutoroi.m_width, newAutoroi.m_height);
                    gtk_combo_box_text_insert_text (GTK_COMBO_BOX_TEXT(gAutoRoiTab->m_autoroiCombo),
                                                    gAutoRoiTab->m_numAutoroisSupported++,
                                                    cValue);
                    gAutoRoiTab->m_supportedAutorois.push_back(newAutoroi);
                    gAutoRoiTab->m_usingNonstandardAutoroi = true;
                } else {
                    // the user 'created' a standard ROI
                    newSelectionIndex = it - gAutoRoiTab->m_supportedAutorois.begin();
                }
                gtk_combo_box_set_active (GTK_COMBO_BOX(gAutoRoiTab->m_autoroiCombo),newSelectionIndex);

            }

            // Update the ROI button
            gAutoRoiTab->loadRoiImage();
            gAutoRoiTab->updateAutoroiButton();
        }
    }

    // Reassert the current value even if we did not succeed in setting it
    sprintf (cValue, "%d",gAutoRoiTab->m_autoroi.m_height);
    gtk_entry_set_text (GTK_ENTRY (gAutoRoiTab->m_height), cValue);
}

extern "C" void AutoroiCenterButtonPressed
    (GtkWidget* widget, GdkEventExpose* event, gpointer userdata )
{
    if (! gCamera || ! gCameraSelectTab || !gAutoRoiTab) return;
    if (gAutoRoiTab->m_numRefreshRequestsOutstanding) return;

    bool disabled = ! gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON(gAutoRoiTab->m_enable));

    PxLAutoLock lock(&gCameraLock);

    PXL_RETURN_CODE rc;

    PXL_ROI newAutoroi = gAutoRoiTab->m_autoroi;

    int newOffsetX = (gAutoRoiTab->m_currentRoi.m_width - newAutoroi.m_width) / 2;
    int newOffsetY = (gAutoRoiTab->m_currentRoi.m_height - newAutoroi.m_height) / 2;

    newAutoroi.m_offsetX = newOffsetX;
    newAutoroi.m_offsetY = newOffsetY;

    rc = gCamera->setRoiValue(AutoRoi, newAutoroi, disabled);
    if (API_SUCCESS (rc))
    {
        // Read it back again, just in case the camera did some 'tuning' of the values
        gCamera->getRoiValue(AutoRoi, &newAutoroi);

        gAutoRoiTab->m_autoroi = newAutoroi;

        // Update the controls
        char cValue[40];
        sprintf (cValue, "%d",gAutoRoiTab->m_autoroi.m_offsetX);
        gtk_entry_set_text (GTK_ENTRY (gAutoRoiTab->m_offsetX), cValue);
        sprintf (cValue, "%d",gAutoRoiTab->m_autoroi.m_offsetY);
        gtk_entry_set_text (GTK_ENTRY (gAutoRoiTab->m_offsetY), cValue);

        // Update the ROI button
        gAutoRoiTab->loadRoiImage();
        gAutoRoiTab->updateAutoroiButton();
    }
}

extern "C" bool AutoroiButtonPress
    (GtkWidget* widget, GdkEventButton *event )
{
    if (! gCamera || ! gAutoRoiTab) return true;
    if (gAutoRoiTab->m_numRefreshRequestsOutstanding) return true;

    // Pressing the button will 'start' an operation, and releasing it will end one
    if (event->type == GDK_BUTTON_PRESS)
    {
        gAutoRoiTab->startAutoroiButtonOperation(widget, event->x, event->y, event->x_root, event->y_root);
    } else {
        gAutoRoiTab->finishAutoroiButtonOperation(widget);
    }

    return true;
}

extern "C" bool AutoroiButtonMotion
    (GtkWidget* widget, GdkEventButton *event )
{
    if (! gCamera || ! gAutoRoiTab) return true;
    if (gAutoRoiTab->m_numRefreshRequestsOutstanding) return true;

    //
    // Step 1
    //      If we are not performing an operation, than all we need to
    //      do, is set the cursor type
    if (gAutoRoiTab->m_currentOp == OP_NONE)
    {
        gdk_window_set_cursor (gtk_widget_get_window (widget),
                               gAutoRoiTab->getCursorForOperation ( gAutoRoiTab->determineOperationFromMouse (event->x, event->y)));
        return true;
    }

    //
    // Step 2
    //      We are performing some sort of operation IE, the user is dragging the mouse.  We need to redraw
    //      the ROI button as the user does the drag
    int deltaX = (int) (event->x_root -  gAutoRoiTab->m_mouseXOnOpStart);
    int deltaY = (int) (event->y_root -  gAutoRoiTab->m_mouseYOnOpStart);

    // define some local variables just to make the code more compact and readable
    int *width = &gAutoRoiTab->m_autoroiButtonRoi.m_width;
    int *height = &gAutoRoiTab->m_autoroiButtonRoi.m_height;
    int *x = &gAutoRoiTab->m_autoroiButtonRoi.m_offsetX;
    int *y = &gAutoRoiTab->m_autoroiButtonRoi.m_offsetY;
    int xMin = gAutoRoiTab->m_autoroiButtonLimits.xMin;
    int xMax = gAutoRoiTab->m_autoroiButtonLimits.xMax;
    int yMin = gAutoRoiTab->m_autoroiButtonLimits.yMin;
    int yMax = gAutoRoiTab->m_autoroiButtonLimits.yMax;
    int widthStart =  gAutoRoiTab->m_autoroiButtonOnOpStart.m_width;
    int heightStart = gAutoRoiTab->m_autoroiButtonOnOpStart.m_height;
    int xStart =  gAutoRoiTab->m_autoroiButtonOnOpStart.m_offsetX;
    int yStart = gAutoRoiTab->m_autoroiButtonOnOpStart.m_offsetY;

    switch (gAutoRoiTab->m_currentOp)
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
        gAutoRoiTab->setAutoroiButtonSize();
        break;
    case OP_RESIZE_TOP_RIGHT:
        *y = min (max (yMin, yStart + deltaY), yStart + heightStart - ROI_MIN_HEIGHT);
        gtk_widget_set_margin_top (widget, *y);
        *width = min (max (ROI_MIN_WIDTH, widthStart + deltaX), xMax - xStart);
        *height = min (max (ROI_MIN_HEIGHT, heightStart - deltaY), yStart + heightStart - yMin);
        gAutoRoiTab->setAutoroiButtonSize();
        break;
    case OP_RESIZE_BOTTOM_RIGHT:
        *width = min (max (ROI_MIN_WIDTH, widthStart + deltaX), xMax - xStart);
        *height = min (max (ROI_MIN_HEIGHT, heightStart + deltaY), yMax - yStart);
        gAutoRoiTab->setAutoroiButtonSize();
        break;
    case OP_RESIZE_BOTTOM_LEFT:
        *x = min (max (xMin, xStart + deltaX), xStart + widthStart - ROI_MIN_WIDTH);
        gtk_widget_set_margin_left (widget, *x);
        *width = min (max (ROI_MIN_WIDTH, widthStart - deltaX), xStart + widthStart - xMin);
        *height = min (max (ROI_MIN_HEIGHT, heightStart + deltaY), yMax - yStart);
        gAutoRoiTab->setAutoroiButtonSize();
        break;
    case OP_RESIZE_TOP:
        *y = min (max (yMin, yStart + deltaY), yStart + heightStart - ROI_MIN_HEIGHT);
        gtk_widget_set_margin_top (widget, *y);
        *height = min (max (ROI_MIN_HEIGHT, heightStart - deltaY), yStart + heightStart - yMin);
        gAutoRoiTab->setAutoroiButtonSize();
        break;
    case OP_RESIZE_RIGHT:
        *width = min ( max (ROI_MIN_WIDTH, widthStart + deltaX), xMax - xStart);
        gAutoRoiTab->setAutoroiButtonSize();
        break;
    case OP_RESIZE_BOTTOM:
        *height = min ( max (ROI_MIN_HEIGHT, heightStart + deltaY), yMax - yStart);
        gAutoRoiTab->setAutoroiButtonSize();
        break;
    case OP_RESIZE_LEFT:
        *x = min (max (xMin, xStart + deltaX), xStart + widthStart - ROI_MIN_WIDTH);
        gtk_widget_set_margin_left (widget, *x);
        *width = min (max (ROI_MIN_WIDTH, widthStart - deltaX), xStart + widthStart - xMin);
        gAutoRoiTab->setAutoroiButtonSize();
        break;
    default:
    case OP_NONE:
        // Just for completeness
        break;
    }

    return true;
}

extern "C" bool AutoroiButtonEnter
    (GtkWidget* widget, GdkEventButton *event )
{
    if (! gCamera || ! gAutoRoiTab) return true;
    if (gAutoRoiTab->m_numRefreshRequestsOutstanding) return true;

    // TEMP PEC +++
    //      NOTE.  I don't think I even need this event.
    // ---
    // If we are performing an operation (the user has the button pressed), then don't take any
    // action on leave/enter -- wait for the user to complete.
    if (gAutoRoiTab->m_currentOp != OP_NONE) return TRUE;

    // The user is not performing an operation.
    //gdk_window_set_cursor (gtk_widget_get_window (widget),
    //                       gAutoRoiTab->getCursorForOperation (gAutoRoiTab->determineOperationFromMouse (event->x, event->y)));

    return true;
}

extern "C" bool AutoroiEnableToggled
    (GtkWidget* widget, GdkEventButton *event )
{
    if (! gCamera || ! gAutoRoiTab) return true;
    if (gAutoRoiTab->m_numRefreshRequestsOutstanding) return true;

    bool enabling = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON(gAutoRoiTab->m_enable));
    bool enabled = false;

    gCamera->setRoiValue(AutoRoi, gAutoRoiTab->m_autoroi, ! enabling);

    enabled = gCamera->enabled (FEATURE_AUTO_ROI);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(gAutoRoiTab->m_enable), enabled);

    return true;
}

extern "C" void AutoRoiExposureOneTimeButtonPressed
    (GtkWidget* widget, GdkEventExpose* event, gpointer userdata )
{
    if (! gCamera || ! gCameraSelectTab || !gAutoRoiTab) return;
    if (!gCamera->oneTimeSuppored(FEATURE_EXPOSURE)) return;
    if (gAutoRoiTab->m_numRefreshRequestsOutstanding) return;

    PxLAutoLock lock(&gCameraLock);

    gOnetimeDialog->initiate(FEATURE_EXPOSURE, 500); // Pool every 500 ms

    // Also add a poller so that the edit control is also updated as the
    // one time is performed.
    gCamera->m_poller->pollAdd(exposureFuncs);

    // Update other tabs the next time they are activated
    gControlsTab->refreshRequired(false);
#if defined VIDEO_CAPTURE_SUPPORTED
    gVideoTab->refreshRequired(false);
#endif
    gLinkTab->refreshRequired(false);
}

extern "C" void AutoRoiGainOneTimeButtonPressed
    (GtkWidget* widget, GdkEventExpose* event, gpointer userdata )
{
    if (! gCamera || ! gCameraSelectTab || !gAutoRoiTab) return;
    if (!gCamera->oneTimeSuppored(FEATURE_GAIN)) return;
    if (gAutoRoiTab->m_numRefreshRequestsOutstanding) return;

    PxLAutoLock lock(&gCameraLock);

    gOnetimeDialog->initiate(FEATURE_GAIN, 500); // Pool every 500 ms

    // Also add a poller so that the edit control is also updated as the
    // one time is performed.
    gCamera->m_poller->pollAdd(gainFuncs);

    // Update other tabs the next time they are activated
    gControlsTab->refreshRequired(false);
}

extern "C" void AutoRoiWhitebalanceOneTimeButtonPressed
    (GtkWidget* widget, GdkEventExpose* event, gpointer userdata )
{
    if (! gCamera || ! gCameraSelectTab || !gAutoRoiTab) return;
    if (!gCamera->oneTimeSuppored(FEATURE_WHITE_SHADING)) return;
    if (gAutoRoiTab->m_numRefreshRequestsOutstanding) return;

    PxLAutoLock lock(&gCameraLock);

    gOnetimeDialog->initiate(FEATURE_WHITE_SHADING, 500); // Pool every 500 ms

    // Also add a poller so that the edit control is also updated as the
    // one time is performed.
    gCamera->m_poller->pollAdd(whitebalanceFuncs);

    // Update other tabs the next time they are activated
    gControlsTab->refreshRequired(false);
}

