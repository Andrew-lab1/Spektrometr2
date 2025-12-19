
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

#include <gtk/gtk.h>
#include <stdlib.h>
#include <algorithm>
#include "featurePoller.h"
#include "camera.h"
#include "captureOEM.h"

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
    // Even though we don't do any camera operations, we need a mutex to ensure the various poll operations are
    // thread safe.  Introducing another mutex runs the risk of deadlock, so use the existing mutex
    PxLAutoLock lock(&gCameraLock);

    // Don't add this one if has already been added
    if (find (m_pollList.begin(), m_pollList.end(), functions) !=  m_pollList.end()) return;
    m_pollList.push_back (functions);

}

void PxLFeaturePoller::pollRemove (const PxLFeaturePollFunctions& functions)    // Remove a feature from the poll list
{
    // Even though we don't do any camera operations, we need a mutex to ensure the various poll operations are
    // thread safe.  Introducing another mutex runs the risk of deadlock, so use the existing mutex
    PxLAutoLock lock(&gCameraLock);

    vector<PxLFeaturePollFunctions>::iterator it;

    it = find (m_pollList.begin(), m_pollList.end(), functions);
    // Don't remove it if it is not there
    if (it ==  m_pollList.end()) return;
    m_pollList.erase (it);
}

bool PxLFeaturePoller::polling (const PxLFeaturePollFunctions& functions) // are we polling thi feature??
{
    return (find (m_pollList.begin(), m_pollList.end(), functions) !=  m_pollList.end());
}


static gboolean updateFeatureControls (gpointer pData)
{
    // Even though we don't do any camera operations, we need a mutex to ensure the various poll operations are
    // thread safe.  Introducing another mutex runs the risk of deadlock, so use the existing mutex
    PxLAutoLock lock(&gCameraLock);

    // Bugzilla.1335 -- Don't need to update the controls if the camera is gone.
    if (! gCamera) return false;

    PxLFeaturePoller *poller = (PxLFeaturePoller *)pData;
    vector<PxLFeaturePollFunctions>::iterator it;

    // update the controls for each of the features.  Note that calling updateControls
    // will remove the item if it's no longer needed, so we need to be careful to
    // not miss any
    int numFuncsPrev = poller->m_pollList.size();
    int numFuncsNow  = numFuncsPrev;
    for (int i = 0; i < numFuncsNow; )
    {
        (poller->m_pollList[i].m_updateControls)();
        numFuncsNow = poller->m_pollList.size();
        if (numFuncsNow == numFuncsPrev) i++;
        numFuncsPrev = numFuncsNow;
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
        {
            // Even though we don't do any camera operations, we need a mutex to ensure the various poll operations are
            // thread safe.  Introducing another mutex runs the risk of deadlock, so use the existing mutex
            PxLAutoLock lock(&gCameraLock);
            if (i >= poller->m_pollsPerUpdate)
            {
                i = 0; // restart our poll count

                // Get each of the feature values
                for (it = poller->m_pollList.begin(); it != poller->m_pollList.end(); it++)
                {
                    if (poller->m_pollList.empty()) break;
                    (*it->m_pollFeature)();
                }
                if (!poller->m_pollList.empty())
                {
                    gdk_threads_add_idle ((GSourceFunc)updateFeatureControls, poller);
                }
            }
        }

        usleep (poller->m_pollInterval*1000);  // stall for a bit -- but convert ms to us
    }

    return NULL;
}




