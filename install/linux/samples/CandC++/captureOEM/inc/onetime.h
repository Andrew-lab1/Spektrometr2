
/***************************************************************************
 *
 *     File: onetime.h
 *
 *     Description: Controls onetime camera operations, including the dialog
 *                  that allows the user to cancel / quit
 *
 */

#if !defined(PIXELINK_ONETIME_H)
#define PIXELINK_ONETIME_H

#include <gtk/gtk.h>
#include "PixeLINKApi.h"

class PxLOnetime
{
public:
    // Constructor
	PxLOnetime (GtkBuilder *builder);
	// Destructor
	~PxLOnetime ();

    void initiate(ULONG feature, ULONG polInterval, float min = 0, float max = 0);
    bool inProgress();
    void updateDialog(ULONG feature, ULONG pollNum, float value);

    GtkWidget  *m_windowOnetime;

	GtkWidget  *m_featureCtl;
	GtkWidget  *m_pollCtl;
	GtkWidget  *m_valueCtl;

    bool    m_onetimeThreadRunning;
    ULONG   m_feature;
    ULONG   m_pollNum;

private:

	const char* featureStr(ULONG feature);
    ULONG m_pollInterval;

};

inline const char* PxLOnetime::featureStr (ULONG feature)
{
    switch (feature)
    {
        case FEATURE_SHUTTER:        return "Exposure";
        case FEATURE_WHITE_SHADING : return "White Balance";
        default:                     return "Unknown";
    }
}

inline bool PxLOnetime::inProgress()
{
   return m_onetimeThreadRunning;
}

#endif // !defined(PIXELINK_ONETIME_H)
