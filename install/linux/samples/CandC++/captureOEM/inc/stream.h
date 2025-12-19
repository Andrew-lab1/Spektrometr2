
/***************************************************************************
 *
 *     File: stream.h
 *
 *     Description:
 *         Controls for the 'Stream' tab  in CaptureOEM.
 *
 */

#if !defined(PIXELINK_STREAM_H)
#define PIXELINK_STREAM_H

#include <vector>
#include <gtk/gtk.h>
#include "tab.h"
#include "roi.h"
#include "pixelFormat.h"
#include "pixelAddress.h"

#define FFOV_AREA_WIDTH  512 // Must match the size in the Glade project
#define FFOV_AREA_HEIGHT 384 // Must match the size in the Glade project

// For reasons I don't understand, GDK does not allow me to create windows that are
// very small.  I discovered these values empirically.  I need to revisit this.
#define ROI_MIN_WIDTH 16
#define ROI_MIN_HEIGHT 28

// Only display the label in the ROI button, if there is room
#define ROI_BUTTON_LABEL_THRESHOLD_WIDTH  64
#define ROI_BUTTON_LABEL_THRESHOLD_HEIGHT 40

typedef enum _FLIP_TYPE
{
    FLIP_NONE = 0,
    FLIP_HORIZONTAL,
    FLIP_VERTICAL,
    FLIP_BOTH
} FLIP_TYPE;

class PxLStream : public PxLTab
{
public:

    // Constructor
    PxLStream (GtkBuilder *builder);
	// Destructor
	~PxLStream ();

	void activate ();   // the user has selected this tab
    void deactivate (); // the user has un-selected this tab
    void refreshRequired (bool noCamera);  // Camera status has changed, requiring a refresh of controls
    void playChange (bool playing);  // indication that the app has transitioned to/from playing state

    void loadFfovImage (bool streamInterrupted = false);
    void updateRoiButton ();
    void startRoiButtonOperation (GtkWidget *widget, double relativeX, double relativeY, double absoluteX, double absoluteY);
    GdkCursor* getCursorForOperation (ROI_BUTTON_OPS op);
    ROI_BUTTON_OPS determineOperationFromMouse (double relativeX, double relativeY);
    void finishRoiButtonOperation (GtkWidget *widget);
    void setRoiButtonSize ();

    //
    // All of the controls

    GtkWidget    *m_roiCombo;
    GtkWidget    *m_offsetX;
    GtkWidget    *m_offsetY;
    GtkWidget    *m_width;
    GtkWidget    *m_height;
    GtkWidget    *m_center;
    GtkWidget    *m_ffovImage;
    GtkWidget    *m_roiButton;
    GtkWidget    *m_pixelFormat;
    GtkWidget    *m_compression;
    GtkWidget    *m_pixelFormatInterpretation;
    GtkWidget    *m_alpha;
    GtkWidget    *m_pixelAddressingMode;
    GtkWidget    *m_pixelAddressingValue;
    GtkWidget    *m_rotate;
    GtkWidget    *m_flip;

    GdkPixbuf    *m_ffovBuf;  //buffer used for ffov image -- Either from the camera, or a default message
    // ROI button limits.  The values are pixels coordinates within the ffov image
    // work area.  If the sensor has the same aspect ration of FFOV_AREA_WIDTH x FFOV_AREA_HEIGHT,
    // then m_roiButtonLimits will be {0, FFOV_AREA_WIDTH, 0, FFOV_AREA_HEIGHT}.  Otherwise,
    // the ROI button is letterboxed within the ffov image area (centered).
    ROI_LIMITS    m_roiButtonLimits;
    float m_cameraAspectRatio; // camera width / height, flip and rotate transformations have already been applied
    // ROI button ROI.  This is actually scaled representation of the the ROI within the ffov.  Units
    // represent pixels for the button. Note that this representation must respect m_roiButtonLimits.  So,
    // if for instance, if the ROI IS currently the ffov, then m_roiButtonRoi will be:
    //     m_width:   m_roiButtonLimits.xMax-m_roiButtonLimits.xMin
    //     m_height:  m_roiButtonLimits.yMax-m_roiButtonLimits.yMin
    //     m_offsetX: m_roiButtonLimits.xMin
    //     m_offsetY: m_roiButtonLimits.yMin
    // In particular, m_offsetX and m_offsetY will NOT both be 0 if the ffov is not the same aspect ratio
    // as FFOV_AREA_WIDTH x FFOV_AREA_HEIGHT
    PXL_ROI       m_roiButtonRoi;

    // Cursors used by the ROI button.  We create all of them at startup, so that we can quickly transition
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

    // Varaibles used for ROI button operations
    ROI_BUTTON_OPS m_currentOp;
    PXL_ROI        m_roiButtonOnOpStart; // the value of m_roiButtonRoi when a ROI button operation is started
    // Keep track of where the mouse is when an operation is started
    double         m_mouseXOnOpStart;
    double         m_mouseYOnOpStart;
    static const double   m_EdgeSensitivity = 3.0;  // If we are within 3 pixels of the edge, we are resizing

    // Camera specific information that we 'cache' to avoid having to read it a bunch of times.
    PXL_ROI       m_roi;    // flip and rotate transformation have already been applied
    PXL_ROI       m_maxRoi; // flip and rotate transformation have already been applied
    int           m_pixelAddressX;
    int           m_pixelAddressY;

    std::vector<PXL_ROI> m_supportedRois;
    int  m_numRoisSupported;
    bool m_usingNonstandardRoi; // true == we have added a 'custom', non-standard ROI to m_supportedRois
    std::vector<COEM_PIXEL_FORMATS> m_supportedPixelFormats;
    std::vector<COEM_PIXEL_FORMAT_INTERPRETATIONS> m_supportedPixelFormatInterpretations;
    std::vector<COEM_PIXEL_ADDRESS_MODES> m_supportedPixelAddressModes;
    std::vector<COEM_PIXEL_ADDRESS_VALUES> m_supportedPixelAddressValues;

    // transformations cache.
    int    m_rotateValue;
    bool   m_verticalFlip;
    bool   m_horizontalFlip;
};

inline GdkCursor* PxLStream::getCursorForOperation(ROI_BUTTON_OPS op)
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



#endif // !defined(PIXELINK_STREAM_H)
