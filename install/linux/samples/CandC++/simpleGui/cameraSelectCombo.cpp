
/***************************************************************************
 *
 *     File: cameraSelectCombo.cpp
 *
 *     Description: Simple wrapper class for the camera select combo list
 */

#include <stdlib.h>
#include "simpleGui.h"

// Prototype definitions of our static functions
static gboolean rebuildCameraSelectCombo (gpointer pData);
static void *scanThread (PxLCameraSelectCombo *combo);

PxLCameraSelectCombo::PxLCameraSelectCombo (GtkWidget *combo)
: m_csCombo(combo)
, m_selectedCamera(0)
, m_requestedCamera(0)
, m_rebuildInProgress(false)
, m_scanThreadRunning(false)
{
    m_comboCameraList.clear();
	m_connectedCameraList.clear();

	m_scanThreadRunning = true;
	m_scanThread = g_thread_new ("cameraScanThread", (GThreadFunc)scanThread, this);
}

PxLCameraSelectCombo::~PxLCameraSelectCombo ()
{
	m_scanThreadRunning = false;
	g_thread_join(m_scanThread);
    g_thread_unref (m_scanThread);
}

// Returns the camera the user selected, or 0 if the user selected 'No Camera'
ULONG PxLCameraSelectCombo::getSelectedCamera()
{
	gint cameraIndex = gtk_combo_box_get_active (GTK_COMBO_BOX(m_csCombo));

	if (cameraIndex < 1) return 0;

	return (atoi (gtk_combo_box_text_get_active_text (GTK_COMBO_BOX_TEXT(m_csCombo))));
}

PXL_RETURN_CODE PxLCameraSelectCombo::scanForCameras ()
{
    PXL_RETURN_CODE rc = ApiSuccess;
    ULONG numCameras = 0;

    //
    // Step 1
    //      We will construct a new list, so release the old one
    m_connectedCameraList.clear();

    //
    // Step 3
    //      Determine how many cameras are connected, and then get the serial numbers
    rc = PxLGetNumberCameras (NULL, &numCameras);
    if (API_SUCCESS(rc) && numCameras > 0)
    {
    	m_connectedCameraList.resize(numCameras);
        rc = PxLGetNumberCameras (&m_connectedCameraList[0], &numCameras);
        if (!API_SUCCESS(rc))
        {
            //
            // Step 3 (OnError)
            //    Could not get the serial numbers, so empty the list
        	m_connectedCameraList.clear();
        }
    }

    return rc;
}

bool PxLCameraSelectCombo::isConnected (ULONG serialNum)
{
    int cameraIndex;
    int numCameras = m_connectedCameraList.size();

    for (cameraIndex=0; cameraIndex<numCameras; cameraIndex++)
    {
        if (m_connectedCameraList[cameraIndex] == serialNum) break;
    }

    return (cameraIndex < numCameras);
}

// Rebuild the camera select list using the values specified in our camera select
// object.
//
// Design Note:
//   Note that this activity can be done as the result of a user action, or as a result
//   of a camera scan.  We do some 'special' handling when creating the camera list, as in
//   the 'typical case' where a camera is already selected (and still there), we want to update
//   the list without doing a COMPLETE list rebuild.  If we empty the list and then rebuild, we
//   get a slight flicker in the control.  So, rather than doing this, we will leave the current
//   camera alone, and only rebuild the rest of the list.
//
//   To accommodate this, we do the treat the list of cameras to choose from (m_comboCameraList)
//   as follows:
//       - If the list is empty, then m_selectedCamera == 0 and 'No Camera' is displayed on
//         the m_csCombo combo.
//       - if it is not empty, the m_comboCameraList[0] is the currently selected camera
//         (m_selectedCamera)
//       - non-selected cameras are ALWAYS at m_comboCameraList index 1 and above.
//
static gboolean rebuildCameraSelectCombo (gpointer pData)
{
	PxLCameraSelectCombo *combo = (PxLCameraSelectCombo *)pData;
	gchar cameraSerial[40];
	gint  currentListSize = combo->m_comboCameraList.size();
	gint  i;

	bool changingCameras = combo->m_selectedCamera != combo->m_requestedCamera;

	combo->m_rebuildInProgress = true;

	//
	// Step 1
	//		Make the necessary changes to the list of cameras that will be displayed
	//      when the user selects the camera select drop down.

	// Do NOT do a complete rebuild if the user is not changing cameras.  In other words,
	// do a partial rebuild if we have a camera and we are not changing to a new
	// camera.  See Design Note above for details.
	if (!changingCameras && combo->m_selectedCamera != 0)
	{
		// Only rebuild the list for non-active cameras

 		// First, remove all entries from our list, and all of the old non-active ones from the combo box.
		combo->m_comboCameraList.clear();
        for (i = currentListSize; i > 1; i--)
 		{
        	gtk_combo_box_text_remove (GTK_COMBO_BOX_TEXT(combo->m_csCombo),i);
 		}
		// Now, add all of the camera to our list, and all of the non-active ones to our combo box
        combo->m_comboCameraList.push_back(combo->m_requestedCamera);
		for (i = 0; i < (gint)combo->m_connectedCameraList.size(); i++)
 		{
 			if (combo->m_connectedCameraList[i] == combo->m_requestedCamera) continue;
 			combo->m_comboCameraList.push_back (combo->m_connectedCameraList[i]);
    	    sprintf (cameraSerial, "%d", combo->m_connectedCameraList[i]);
    	    gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT(combo->m_csCombo), cameraSerial);
 		}
	 } else {
		// Rebuild the entire list.
		combo->m_comboCameraList.clear();
		gtk_combo_box_text_remove_all (GTK_COMBO_BOX_TEXT(combo->m_csCombo));

		// always have 'No Camera' as our first choice
		gtk_combo_box_text_insert_text (GTK_COMBO_BOX_TEXT(combo->m_csCombo), 0, "No Camera");

		// always have our current camera (if there is one) as our second choice
		if (0 != combo->m_requestedCamera)
		{
			combo->m_comboCameraList.push_back (combo->m_requestedCamera);
			sprintf (cameraSerial, "%d", combo->m_requestedCamera);
			gtk_combo_box_text_insert_text (GTK_COMBO_BOX_TEXT(combo->m_csCombo), 1, cameraSerial);
		}

		gtk_combo_box_set_active (GTK_COMBO_BOX(combo->m_csCombo),(0 == combo->m_requestedCamera ? 0 : 1));
		combo->m_selectedCamera = combo->m_requestedCamera;

		// And finally, add all of our non-active entries
		for (i = 0; i < (gint)combo->m_connectedCameraList.size(); i++)
	    {
	    	if (combo->m_requestedCamera == combo->m_connectedCameraList[i]) continue;
	    	combo->m_comboCameraList.push_back (combo->m_connectedCameraList[i]);
	    	sprintf (cameraSerial, "%d", combo->m_connectedCameraList[i]);
	    	gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT(combo->m_csCombo), cameraSerial);
	    }

	 }

	 //
	// Step 2
	//		If there has been a change in the currently selected camera, then
	//      update all of the other controls.
	 if (changingCameras)
	 {
    	 previewObject->greyAll();
	     if (combo->m_selectedCamera != 0)
	     {
	    	 previewObject->activatePlay();
	     }
	 }

  	 csObject->m_rebuildInProgress = false;

	 return false;  //  Only run once....
}

// thread to periodically scan the bus for PixeLINK cameras.
static void *scanThread (PxLCameraSelectCombo *combo)
{
    ULONG rc = ApiSuccess;

    const ULONG sleepTimeUs = 1000 * 500; // 500 ms
    const ULONG pollsBetweenScans = 10;   // 5 seconds between scans

	// Create our initial (empty) camera list
    combo->m_requestedCamera = 0;
	gdk_threads_add_idle ((GSourceFunc)rebuildCameraSelectCombo, combo);

    for (ULONG i = pollsBetweenScans; combo->m_scanThreadRunning; i++)
    {
        if (i >= pollsBetweenScans)
        {
			i = 0; // restart our poll count
			rc = combo->scanForCameras();
			if (API_SUCCESS(rc))
			{
				if (NULL == pCamera && combo->m_connectedCameraList.size() > 0)
				{
					// There are some cameras, yet we haven't selected one
					// yet.  Simply pick the first one found.
					try
					{
						pCamera = new PxLCamera (combo->m_connectedCameraList[0]);
					} catch (PxLError& e) {
						if (e.m_rc == ApiNoCameraError)
						{
							printf ("Could not grab camera %d -- still initializing??\n", combo->m_connectedCameraList[0]);
							continue;
						}
						printf ("%s\n", e.showReason());
						combo->m_scanThreadRunning = false;
						break;
					}
					printf ("Grabbed camera %d\n",pCamera->serialNum());
				    combo->m_requestedCamera = pCamera->serialNum();
					gdk_threads_add_idle ((GSourceFunc)rebuildCameraSelectCombo, combo);
				} else if (NULL != pCamera && ! combo->isConnected(pCamera->serialNum())) {
					// The camera that we had is gone !!
					printf ("Released camera %d\n",pCamera->serialNum());
					delete pCamera;
					pCamera = NULL;
				    combo->m_requestedCamera = 0;
					gdk_threads_add_idle ((GSourceFunc)rebuildCameraSelectCombo, combo);
					//continue;  // Try another scan immediately, as there may already be one connected.
				} else if (NULL != pCamera) {
					combo->m_requestedCamera = pCamera->serialNum();
					gdk_threads_add_idle ((GSourceFunc)rebuildCameraSelectCombo, combo);
				}
			}
        }
        usleep (sleepTimeUs);  // wait a bit before we wake again.
    }

	// We are about to exit -- release the camera (if we have one)
    if (NULL != pCamera)
    {
    	printf ("Released camera %d\n",pCamera->serialNum());
		delete pCamera;
		pCamera = NULL;
    }

    return NULL;
}

extern "C" void NewCameraSelected
    (GtkWidget* widget, GdkEventExpose* event, gpointer userdata )
{
	ULONG selectedCamera;

	// this 'handler' gets called as we rebuild the list -- ignore these ones
	// as we are only intereseted in user input
	if (csObject->m_rebuildInProgress) return;


	selectedCamera = csObject->getSelectedCamera();

	if (0 == selectedCamera)
	{
		if (NULL != pCamera)
		{
			// The user doesn't want this camera anymore
	    	printf ("Releasing camera %d\n",pCamera->serialNum());
    		delete pCamera;
    		pCamera = NULL;
		}
	} else {
		// The user selected a camera
               if (pCamera && (pCamera->serialNum() != selectedCamera))
               {
                    // The user selected a different camera -- release the old one
                    printf ("Released camera %d\n",pCamera->serialNum());
                    delete pCamera;
                    pCamera = NULL;
               }

	       try
               {
	            pCamera = new PxLCamera (selectedCamera);
                    printf ("Grabbed camera %d\n",selectedCamera);
	       } catch (PxLError& e) {
	            printf ("%s\n", e.showReason());
               }
	}
	csObject->m_requestedCamera = selectedCamera;
	gdk_threads_add_idle ((GSourceFunc)rebuildCameraSelectCombo, csObject);
}




