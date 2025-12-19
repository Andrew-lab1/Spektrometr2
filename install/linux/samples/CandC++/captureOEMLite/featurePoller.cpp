
/***************************************************************************
 *
 *     File: featurePoller.cpp
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
*/

#include <stdlib.h>
#include <algorithm>
#include "captureOEMLite.h"

using namespace std;

// Prototype definitions of our static functions
static gboolean updateFeatureControls (gpointer pData);
static void *pollThread (PxLFeaturePoller *poller);

PxLFeaturePollFunctions::PxLFeaturePollFunctions (PXL_POLL_FEATURE_FUNCTION pollFeature, PXL_UPDATE_CONTROLS_FUNCTION updateControls)
: m_pollFeature(pollFeature)
, m_updateControls(updateControls)
{}

bool PxLFeaturePollFunctions::operator==(const PxLFeaturePollFunctions& func)
{
    return (func.m_pollFeature == m_pollFeature &&
            func.m_updateControls == m_updateControls);
}


// updateInterval is the number of milliseconds between each update of the features being polled
PxLFeaturePoller::PxLFeaturePoller (ULONG updateInterval)
: m_pollThreadRunning(false)
{
    m_pollList.clear();
    m_pollsPerUpdate = updateInterval / m_pollInterval;

	m_pollThreadRunning = true;
	m_pollThread = g_thread_new ("featurePollThread", (GThreadFunc)pollThread, this);
}

PxLFeaturePoller::~PxLFeaturePoller ()
{
	m_pollThreadRunning = false;
	g_thread_join(m_pollThread);
    g_thread_unref (m_pollThread);

    m_pollList.clear();
}

void PxLFeaturePoller::pollAdd (const PxLFeaturePollFunctions& functions)    // Add a feature to the poll list
{
    vector<PxLFeaturePollFunctions>::iterator it;

    // Don't add this one if has already been added
    if (find (m_pollList.begin(), m_pollList.end(), functions) !=  m_pollList.end()) return;
    m_pollList.push_back (functions);
}

void PxLFeaturePoller::pollRemove (const PxLFeaturePollFunctions& functions)    // Remove a feature from the poll list
{
    vector<PxLFeaturePollFunctions>::iterator it;

    it = find (m_pollList.begin(), m_pollList.end(), functions);
    // Don't remove it if it is not there
    if (it ==  m_pollList.end()) return;
    m_pollList.erase (it);
}

static gboolean updateFeatureControls (gpointer pData)
{
    PxLFeaturePoller *poller = (PxLFeaturePoller *)pData;
    vector<PxLFeaturePollFunctions>::iterator it;

    // update the controls for each of the features
    for (it = poller->m_pollList.begin(); it != poller->m_pollList.end(); it++)
    {
        (*it->m_updateControls)();
    }

    return false;  //  Only run once....
}

// thread to periodically poll the active cameras, getting specified features values so the controls
// can be updated..
static void *pollThread (PxLFeaturePoller *poller)
{
    vector<PxLFeaturePollFunctions>::iterator it;

    for (ULONG i = 0; poller->m_pollThreadRunning; i++)
    {
        if (i >= poller->m_pollsPerUpdate)
        {
			i = 0; // restart our poll count

			// Get each of the feature values
			for (it = poller->m_pollList.begin(); it != poller->m_pollList.end(); it++)
			{
	            (*it->m_pollFeature)();
			}
			if (!poller->m_pollList.empty())
			{
		        gdk_threads_add_idle ((GSourceFunc)updateFeatureControls, poller);
			}
        }

        usleep (poller->m_pollInterval*1000);  // stall for a bit -- but convert ms to us
    }

    return NULL;
}




