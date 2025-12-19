
/***************************************************************************
 *
 *     File: pixelFormat.cpp
 *
 *     Description: All pixelFormat controls.
 */

#include "captureOEMLite.h"
#include <algorithm>

using namespace std;

PxLPixelFormat::PxLPixelFormat (GtkWidget *combo)
: m_combo(combo)
{
}

PxLPixelFormat::~PxLPixelFormat ()
{
}

// Disables all exposure controls/
void PxLPixelFormat:: greyAll()
{
    gtk_combo_box_text_remove_all (GTK_COMBO_BOX_TEXT(m_combo));
	gtk_widget_set_sensitive (m_combo, false);
}

// enables all supported pixel format controls, and initializes them to current values.
void PxLPixelFormat:: initialize()
{
    PxLAutoLock lock(&pCameraLock);

    m_comboEntries.clear();
    if (pCamera)
    {
        if (pCamera->supported(FEATURE_PIXEL_FORMAT))
        {
            float min, max, currentValue;
            bool restoreRequired = false;
            PXL_RETURN_CODE rc;

            gtk_widget_set_sensitive (m_combo, true);
            pCamera->getRange(FEATURE_PIXEL_FORMAT, &min, &max);
            pCamera->getValue(FEATURE_PIXEL_FORMAT, &currentValue);

            // We know the camera support the 'min' pixel format
            gtk_combo_box_text_insert_text (GTK_COMBO_BOX_TEXT(m_combo),
                                            fromApi(min),
                                            PxLFormats[fromApi(min)]);
            m_comboEntries.push_back(fromApi(min));

            // The pixel formats between the min and max are a little tougher.  The
            // camera may not support all values in the range.  The only way to see
            // which of the ones the middle that are supported, is to try them.
            for (float candidate = min+1.0; candidate < max; candidate+=1.0)
            {
                if (find (m_comboEntries.begin(),
                          m_comboEntries.end(),
                          fromApi(candidate)) != m_comboEntries.end())
                {
                    // This entry is already there.  That can happen as pCamera->setValue will accept
                    // both the generic color description (like PIXEL_FORMAT_BAYER8) and the more
                    // specific color descriptor (like PIXEL_FORMAT_BAYER8_GBRG).
                    continue;
                }
                rc = pCamera->setValue(FEATURE_PIXEL_FORMAT, candidate);
                if (API_SUCCESS(rc)) {
                    restoreRequired = true;
                    gtk_combo_box_text_insert_text (GTK_COMBO_BOX_TEXT(m_combo),
                                                    fromApi(candidate),
                                                    PxLFormats[fromApi(candidate)]);
                    m_comboEntries.push_back(fromApi(candidate));
                }
            }
            // restore the old value (if necessary)
            if (restoreRequired) pCamera->setValue(FEATURE_PIXEL_FORMAT, currentValue);

            // We know the camera support the 'max' pixel format
            if (max > min &&
                find (m_comboEntries.begin(),
                      m_comboEntries.end(),
                      fromApi(max)) == m_comboEntries.end())
            {
                gtk_combo_box_text_insert_text (GTK_COMBO_BOX_TEXT(m_combo),
                                                fromApi(max),
                                                PxLFormats[fromApi(max)]);
                m_comboEntries.push_back(fromApi(max));
            }

            // And finally, show our current pixel format as the 'active' one
            for (int i=0; i<(int)m_comboEntries.size(); i++)
            {
                if (m_comboEntries[i] != fromApi(currentValue)) continue;
                gtk_combo_box_set_active (GTK_COMBO_BOX(m_combo),i);
                break;
            }
        }
    }
}

extern "C" void NewPixelFormatSelected
    (GtkWidget* widget, GdkEventExpose* event, gpointer userdata )
{
    if (!pCamera) return;

    gint pixelFormatIndex = gtk_combo_box_get_active (GTK_COMBO_BOX(pixelFormatObject->m_combo));

    pCamera->setValue(FEATURE_PIXEL_FORMAT,
                      pixelFormatObject->toApi(pixelFormatObject->m_comboEntries[pixelFormatIndex]));

    // Update our frame rate control, as it's limit have probably changed.
    frameRateObject->updateFrameRate();
}

