/***************************************************************************
 *
 *     File: link.h
 *
 *     Description:
 *         Controls for the 'Link' tab  in CaptureOEM.
 *
 */

#if !defined(PIXELINK_LINK_H)
#define PIXELINK_LINK_H

#include <gtk/gtk.h>
#include <PixeLINKApi.h>
#include "slider.h"
#include "tab.h"

class PxLLink : public PxLTab
{
public:

    // Constructor
    PxLLink (GtkBuilder *builder);
    // Destructor
    ~PxLLink ();

    void activate ();   // the user has selected this tab
    void deactivate (); // the user has un-selected this tab
    void refreshRequired (bool noCamera);  // Camera status has changed, requiring a refresh of controls
    void streamChange (bool streaming); // Cameras streaming state has changed

    //
    // All of the controls

    GtkWidget    *m_maxPacketSize;
    GtkWidget    *m_maxPacketSizeOneTime;
    GtkWidget    *m_bwLimitLabel;
    GtkWidget    *m_bwLimitEnable;
    PxLSlider    *m_bwLimitSlider;

    U32          m_maxPacketSizeLast;

};

#endif // !defined(PIXELINK_LINK_H)
