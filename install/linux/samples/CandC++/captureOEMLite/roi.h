
/***************************************************************************
 *
 *     File: roi.h
 *
 *     Description: Simple wrapper class for all of the Region Of Interest (ROI) controls
 *
 */

#if !defined(PIXELINK_ROI_H)
#define PIXELINK_ROI_H

#include "PixeLINKApi.h"
#include <gtk/gtk.h>
#include <vector>


class PXL_ROI
{
public:
    bool operator==(const PXL_ROI& rhs) {return (rhs.m_width==this->m_width && rhs.m_height==this->m_height);}
    bool operator!=(const PXL_ROI& rhs) {return !operator==(rhs);}
    int m_width;
    int m_height;
};

class PxLRoi
{
public:
    // Constructor
    PxLRoi (GtkWidget *roiCombo);
	// Destructor
	~PxLRoi ();

    void  greyAll ();
    void  initialize ();

    GtkWidget    *m_combo;
    std::vector<PXL_ROI>  m_comboEntries;

};

#endif // !defined(PIXELINK_ROI_H)
