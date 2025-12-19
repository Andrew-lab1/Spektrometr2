
/***************************************************************************
 *
 *     File: autoRoi.h
 *
 *     Description:
 *         Controls for the 'AutoRoi' tab  in CaptureOEM.
 *
 */

#if !defined(PIXELINK_AUTOROI_H)
#define PIXELINK_AUTOROI_H

#include <vector>
#include <gtk/gtk.h>
#include "roi.h"
#include "slider.h"
#include "tab.h"

#define ROI_AREA_WIDTH  384 // Must match the size in the Glade project
#define ROI_AREA_HEIGHT 288 // Must match the size in the Glade project

// For reasons I don't understand, GDK does not allow me to create windows that are
// very small.  I discovered these values empirically.  I need to revisit this.
#define AUTOROI_MIN_WIDTH 16
#define AUTOROI_MIN_HEIGHT 28

// Only display the label in the SSROI button, if there is room
#define AUTOROI_BUTTON_LABEL_THRESHOLD_WIDTH  64
#define AUTOROI_BUTTON_LABEL_THRESHOLD_HEIGHT 40

class PxLAutoRoi : public PxLTab
{
public:

    // Constructor
    PxLAutoRoi (GtkBuilder *builder);
	// Destructor
	~PxLAutoRoi ();

	void activate ();   // the user has selected this tab
    void deactivate (); // the user has un-selected this tab
    void refreshRequired (bool noCamera);  // Camera status has changed, requiring a refresh of controls
    void playChange (bool playing);  // indication that the app has transitioned to/from playing state

    void loadRoiImage (bool streamInterrupted = false);
    void updateAutoroiButton ();
    void startAutoroiButtonOperation (GtkWidget *widget, double relativeX, double relativeY, double absoluteX, double absoluteY);
    GdkCursor* getCursorForOperation (ROI_BUTTON_OPS op);
    ROI_BUTTON_OPS determineOperationFromMouse (double relativeX, double relativeY);
    void finishAutoroiButtonOperation (GtkWidget *widget);
    void setAutoroiButtonSize ();

    float m_exposureLast; // in millisecond units
    float m_gainLast;
    float m_redLast;
    float m_greenLast;
    float m_blueLast;

    //
    // All of the controls

    GtkWidget    *m_enable;
    GtkWidget    *m_autoroiCombo;
    GtkWidget    *m_offsetX;
    GtkWidget    *m_offsetY;
    GtkWidget    *m_width;
    GtkWidget    *m_height;
    GtkWidget    *m_center;
    GtkWidget    *m_roiImage;
    GtkWidget    *m_autoroiButton;
    GtkWidget    *m_exposure;
    GtkWidget    *m_gain;
    GtkWidget    *m_red;
    GtkWidget    *m_green;
    GtkWidget    *m_blue;
    GtkWidget    *m_exposureOneTime;
    GtkWidget    *m_gainOneTime;
    GtkWidget    *m_whiteBalanceOneTime;


    GdkPixbuf    *m_roiBuf;  //buffer used for roi image -- Either from the camera, or a default message
    // AUTOROI button limits.  The values are pixels coordinates within the roi image
    // work area.  If the roi has the same aspect ration of ROI_AREA_WIDTH x ROI_AREA_HEIGHT,
    // then m_autoroiButtonLimits will be {0, ROI_AREA_WIDTH, 0, ROI_AREA_HEIGHT}.  Otherwise,
    // the AUTOROI button is letterboxed within the roi image area (centered).
    ROI_LIMITS    m_autoroiButtonLimits;
    float m_roiAspectRatio; // roi width / height, flip and rotate transformations have already been applied
    // AUTOROI button ROI.  This is actually scaled representation of the the AUTOROI within the roi.  Units
    // represent pixels for the button. Note that this representation must respect m_autoroiButtonLimits.  So,
    // if for instance, if the AUTOROI IS currently the roi, then m_autoroiButtonRoi will be:
    //     m_width:   m_autoroiButtonLimits.xMax-m_autoroiButtonLimits.xMin
    //     m_height:  m_autoroiButtonLimits.yMax-m_autoroiButtonLimits.yMin
    //     m_offsetX: m_autoroiButtonLimits.xMin
    //     m_offsetY: m_autoroiButtonLimits.yMin
    // In particular, m_offsetX and m_offsetY will NOT both be 0 if the roi is not the same aspect ratio
    // as ROI_AREA_WIDTH x ROI_AREA_HEIGHT
    PXL_ROI       m_autoroiButtonRoi;

    // Cursors used by the AUTOROI button.  We create all of them at startup, so that we can quickly transition
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
    PXL_ROI        m_autoroiButtonOnOpStart; // the value of m_autoroiButtonRoi when a AUTOROI button operation is started
    // Keep track of where the mouse is when an operation is started
    double         m_mouseXOnOpStart;
    double         m_mouseYOnOpStart;
    static const double   m_EdgeSensitivity = 3.0;  // If we are within 3 pixels of the edge, we are resizing

    // Camera specific information that we 'cache' to avoid having to read it a bunch of times.
    PXL_ROI       m_currentRoi; // flip and rotate transformation have already been applied
    PXL_ROI       m_autoroi;    // flip and rotate transformation have already been applied
    PXL_ROI       m_maxAutoroi; // flip and rotate transformation have already been applied

    std::vector<PXL_ROI> m_supportedAutorois;
    int  m_numAutoroisSupported;
    bool m_usingNonstandardAutoroi; // true == we have added a 'custom', non-standard ROI to m_supportedRois
};

inline GdkCursor* PxLAutoRoi::getCursorForOperation(ROI_BUTTON_OPS op)
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



#endif // !defined(PIXELINK_AUTOROI_H)
