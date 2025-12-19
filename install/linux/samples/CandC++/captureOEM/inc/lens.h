
/***************************************************************************
 *
 *     File: lens.h
 *
 *     Description:
 *         Controls for the 'Lens' tab  in CaptureOEM.
 *
 */

#if !defined(PIXELINK_LENS_H)
#define PIXELINK_LENS_H

#include <vector>
#include <gtk/gtk.h>
#include "roi.h"
#include "slider.h"
#include "tab.h"

#define ROI_AREA_WIDTH  384 // Must match the size in the Glade project
#define ROI_AREA_HEIGHT 288 // Must match the size in the Glade project

// For reasons I don't understand, GDK does not allow me to create windows that are
// very small.  I discovered these values empirically.  I need to revisit this.
#define SSROI_MIN_WIDTH 16
#define SSROI_MIN_HEIGHT 28

// Only display the label in the SSROI button, if there is room
#define SSROI_BUTTON_LABEL_THRESHOLD_WIDTH  64
#define SSROI_BUTTON_LABEL_THRESHOLD_HEIGHT 40

class PxLLens : public PxLTab
{
public:

    // Constructor
    PxLLens (GtkBuilder *builder);
	// Destructor
	~PxLLens ();

	void activate ();   // the user has selected this tab
    void deactivate (); // the user has un-selected this tab
    void refreshRequired (bool noCamera);  // Camera status has changed, requiring a refresh of controls
    void playChange (bool playing);  // indication that the app has transitioned to/from playing state

    void loadRoiImage (bool streamInterrupted = false);
    void updateSsroiButton ();
    void startSsroiButtonOperation (GtkWidget *widget, double relativeX, double relativeY, double absoluteX, double absoluteY);
    GdkCursor* getCursorForOperation (ROI_BUTTON_OPS op);
    ROI_BUTTON_OPS determineOperationFromMouse (double relativeX, double relativeY);
    void finishSsroiButtonOperation (GtkWidget *widget);
    void setSsroiButtonSize ();

    //
    // All of the controls

    GtkWidget    *m_ssroiCombo;
    GtkWidget    *m_offsetX;
    GtkWidget    *m_offsetY;
    GtkWidget    *m_width;
    GtkWidget    *m_height;
    GtkWidget    *m_center;
    GtkWidget    *m_roiImage;
    GtkWidget    *m_ssroiButton;
    GtkWidget    *m_sharpnessScore;
    GtkWidget    *m_controllerCombo;
    PxLSlider    *m_focusSlider;
    GtkWidget    *m_focusAutoMin;
    GtkWidget    *m_focusSetAutoMin;
    GtkWidget    *m_focusAutoMax;
    GtkWidget    *m_focusSetAutoMax;
    GtkWidget    *m_focusOneTime;
    PxLSlider    *m_zoomSlider;
    GtkWidget    *m_zoomAssertMin;
    GtkWidget    *m_zoomAssertMax;

    GdkPixbuf    *m_roiBuf;  //buffer used for roi image -- Either from the camera, or a default message
    // SSROI button limits.  The values are pixels coordinates within the roi image
    // work area.  If the roi has the same aspect ration of ROI_AREA_WIDTH x ROI_AREA_HEIGHT,
    // then m_ssroiButtonLimits will be {0, ROI_AREA_WIDTH, 0, ROI_AREA_HEIGHT}.  Otherwise,
    // the SSROI button is letterboxed within the roi image area (centered).
    ROI_LIMITS    m_ssroiButtonLimits;
    float m_roiAspectRatio; // roi width / height, flip and rotate transformations have already been applied
    // SSROI button ROI.  This is actually scaled representation of the the SSROI within the roi.  Units
    // represent pixels for the button. Note that this representation must respect m_ssroiButtonLimits.  So,
    // if for instance, if the SSROI IS currently the roi, then m_ssroiButtonRoi will be:
    //     m_width:   m_ssroiButtonLimits.xMax-m_ssroiButtonLimits.xMin
    //     m_height:  m_ssroiButtonLimits.yMax-m_ssroiButtonLimits.yMin
    //     m_offsetX: m_ssroiButtonLimits.xMin
    //     m_offsetY: m_ssroiButtonLimits.yMin
    // In particular, m_offsetX and m_offsetY will NOT both be 0 if the roi is not the same aspect ratio
    // as ROI_AREA_WIDTH x ROI_AREA_HEIGHT
    PXL_ROI       m_ssroiButtonRoi;

    // Cursors used by the SSROI button.  We create all of them at startup, so that we can quickly transition
    // from one to another
    GdkCursor *m_originalCursor;
    GdkCursor *m_moveCursor;
    GdkCursor *m_tlCursor;
    GdkCursor *m_trCursor;
    GdkCursor *m_blCursor;
    GdkCursor *m_brCursor;
    GdkCursor *m_tCursor;
    GdkCursor *m_rCursor;
    GdkCursor *m_bCursor;
    GdkCursor *m_lCursor;

    // Variables used for ROI button operations
    ROI_BUTTON_OPS m_currentOp;
    PXL_ROI        m_ssroiButtonOnOpStart; // the value of m_ssroiButtonRoi when a SSROI button operation is started
    // Keep track of where the mouse is when an operation is started
    double         m_mouseXOnOpStart;
    double         m_mouseYOnOpStart;
    static const double   m_EdgeSensitivity = 3.0;  // If we are within 3 pixels of the edge, we are resizing

    // Camera specific information that we 'cache' to avoid having to read it a bunch of times.
    PXL_ROI       m_currentRoi; // flip and rotate transformation have already been applied
    PXL_ROI       m_ssroi;      // flip and rotate transformation have already been applied
    PXL_ROI       m_maxSsroi;   // flip and rotate transformation have already been applied
    float         m_maxSs;      // The maximum Sharpness Score possible for the current Ssroi

    std::vector<PXL_ROI> m_supportedSsrois;
    int  m_numSsroisSupported;
    bool m_usingNonstandardSsroi; // true == we have added a 'custom', non-standard ROI to m_supportedRois

    float m_focusLast;  // The focus value most recently read
    float m_ssLast;     // The sharpness score of the most recent callback
};

inline GdkCursor* PxLLens::getCursorForOperation(ROI_BUTTON_OPS op)
{
    switch (op)
    {
    case OP_MOVE: return m_moveCursor;
    case OP_RESIZE_TOP_LEFT: return m_tlCursor;
    case OP_RESIZE_TOP_RIGHT: return m_trCursor;
    case OP_RESIZE_BOTTOM_RIGHT: return m_brCursor;
    case OP_RESIZE_BOTTOM_LEFT: return m_blCursor;
    case OP_RESIZE_TOP: return m_tCursor;
    case OP_RESIZE_RIGHT: return m_rCursor;
    case OP_RESIZE_BOTTOM: return m_bCursor;
    case OP_RESIZE_LEFT: return m_lCursor;
    default:
    case OP_NONE : return m_originalCursor;
    }

    return m_originalCursor;
}



#endif // !defined(PIXELINK_LENS_H)
