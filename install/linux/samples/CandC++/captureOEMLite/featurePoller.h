
/***************************************************************************
 *
 *     File: featurePoller.h
 *
 *     Description:
 *       Class definition for our poll object that monitors camera features (in
 *       continuous auto mode) for changes.
 *
 *       Specifically, a feature whose value can change autonomously (by the cmaera)
 *       can register 2 functions with this object:
 *          - A function that will we called to query the camera for the current
 *            feature value
 *          - A function that will be called to update the features controls to
 *            the value in use by the camera.
 *
 */

#if !defined(PIXELINK_FEATURE_POLLER_H)
#define PIXELINK_FEATURE_POLLER_H

#include "PixeLINKApi.h"
#include <vector>
#include <gtk/gtk.h>

// data types/class for the following functions:
//   1. that we use to check specific features to see if they have changed, and
//   2. that we call to update the user (GUI) control for that feature
typedef PXL_RETURN_CODE ( * PXL_POLL_FEATURE_FUNCTION)();
typedef void ( * PXL_UPDATE_CONTROLS_FUNCTION)();

class PxLFeaturePollFunctions
{
public:
    PxLFeaturePollFunctions (PXL_POLL_FEATURE_FUNCTION pollFeature, PXL_UPDATE_CONTROLS_FUNCTION updateControls);
    bool operator==(const PxLFeaturePollFunctions&);

    PXL_POLL_FEATURE_FUNCTION    m_pollFeature;    // Check the feature value (on the camera)
    PXL_UPDATE_CONTROLS_FUNCTION m_updateControls; // Update the user controls
};

class PxLFeaturePoller
{
public:
    // Constructor
    // updateInterval is the number of milliseconds between each update of the features being polled
    PxLFeaturePoller (ULONG updateInterval);
	// Destructor
	~PxLFeaturePoller ();

	void pollAdd (const PxLFeaturePollFunctions& functions);    // Add a feature to the poll list
    void pollRemove (const PxLFeaturePollFunctions& functions); // remove a feature from the poll list

    std::vector<PxLFeaturePollFunctions> m_pollList;     // The set of features requiring polling

    static const ULONG m_pollInterval = 200; //200 ms between polls ensures the poll thread will exit quickly
    ULONG m_pollsPerUpdate;

    bool     m_pollThreadRunning;
    GThread *m_pollThread;
};

#endif // !defined(PIXELINK_FEATURE_POLLER_H)
