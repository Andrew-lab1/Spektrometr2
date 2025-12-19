
/***************************************************************************
 *
 *     File: roi.cpp
 *
 *     Description: All Region Of Interest (ROI) controls.
 */

#include "captureOEMLite.h"

using namespace std;

static PXL_ROI standardRois[] =
{
        //  width,  height,
        {   32,     32    },
        {   64,     64    },
        {   128,    128   },
        {   256,    256   },
        {   320,    240   },
        {   640,    480   },
        {   800,    600   },
        {   1024,   768   },
        {   1280,   1024  },
        {   1600,   1200  },
        {   1920,   1080  },
        {   2560,   1440  },
        {   4096,   2160  }
};

PxLRoi::PxLRoi (GtkWidget *combo)
: m_combo(combo)
{
}

PxLRoi::~PxLRoi ()
{
}

// Disables all exposure controls/
void PxLRoi:: greyAll()
{
    gtk_combo_box_text_remove_all (GTK_COMBO_BOX_TEXT(m_combo));
	gtk_widget_set_sensitive (m_combo, false);
}

// enables all supported roi controls, and initializes them to current values.
void PxLRoi:: initialize()
{
    PxLAutoLock lock(&pCameraLock);

    //
    // Step 1
    //      Start with empty roi control.  Only attempt to populate
    //      them if we have a camera that supports roi
    m_comboEntries.clear();
    if (pCamera)
    {
        if (pCamera->supported(FEATURE_ROI))
        {
            PXL_ROI min, max, current;
            bool restoreRequired = false;
            PXL_RETURN_CODE rc;
            gchar roiText[40];

            bool  autoFrameRate = true;
            float frameRate;

            //
            // Step 2
            //      we have a camera that supports roi.  Find the range
            //      of values supported
            gtk_widget_set_sensitive (m_combo, true);

            pCamera->getRoiRange(&min, &max);
            pCamera->getRoiValue(&current);
            // Bugzilla.676.  Adjusting the ROI might lead to a change in frame rate.  So read
            // the current frameRateInformation.
            pCamera->getContinuousAuto(FEATURE_FRAME_RATE, &autoFrameRate);
            pCamera->getValue(FEATURE_FRAME_RATE, &frameRate);

            //
            // Step 3
            //      Walk though each of our 'standard' ROIs, to see which ones are
            //      supported.
            int numRoisSupported = 0;
            // Don't bother with this step if we only support the one ROI (it's not settable)
            if (min.m_width < max.m_width && min.m_height < max.m_height)
            {

                for (ULONG i=0; i<(sizeof(standardRois)/sizeof(standardRois[0])); i++)
                {
                    rc = pCamera->setRoiValue(standardRois[i]);
                    if (API_SUCCESS(rc))
                    {
                        if (standardRois[i] != current) restoreRequired = true;
                        sprintf (roiText, "%d x %d", standardRois[i].m_width, standardRois[i].m_height);
                        gtk_combo_box_text_insert_text (GTK_COMBO_BOX_TEXT(m_combo),
                                                        numRoisSupported++,
                                                        roiText);
                        m_comboEntries.push_back(standardRois[i]);
                    }
                }

            }

            //
            // Step 4
            //      We always support the max value (full ROI).  So, if we have not already added it,
            //      it, do so.
            if (m_comboEntries.empty() || m_comboEntries.back() != max)
            {
                if (max != current) restoreRequired = true;
                sprintf (roiText, "%d x %d", max.m_width, max.m_height);
                gtk_combo_box_text_insert_text (GTK_COMBO_BOX_TEXT(m_combo),
                                                numRoisSupported++,
                                                roiText);
                m_comboEntries.push_back(max);
            }

            //
            // Step 5.
            //      If we changed the roi in step 3 or 4, then restore the
            //      restore the old roi and frame rate
            if (restoreRequired)
            {
                pCamera->setRoiValue(current);
                if (autoFrameRate)
                {
                    pCamera->setContinuousAuto(FEATURE_FRAME_RATE, true);
                } else {
                    pCamera->setValue(FEATURE_FRAME_RATE, frameRate);
                }
            }

            //
            // Step 7.
            //      And finally, show our current roi as the 'active' one
            for (int i=0; i<(int)m_comboEntries.size(); i++)
            {
                if (m_comboEntries[i] != current) continue;
                gtk_combo_box_set_active (GTK_COMBO_BOX(m_combo),i);
                break;
            }
        }
    }
}

extern "C" void NewRoiSelected
    (GtkWidget* widget, GdkEventExpose* event, gpointer userdata )
{
    if (! pCamera || ! csObject ) return;
    if (csObject->changingCameras()) return;

    gint index = gtk_combo_box_get_active (GTK_COMBO_BOX(roiObject->m_combo));

    pCamera->setRoiValue(roiObject->m_comboEntries[index]);

    // Update our frame rate control, as it's limit have probably changed.
    frameRateObject->updateFrameRate();
}

