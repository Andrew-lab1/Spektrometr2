
/***************************************************************************
 *
 *     File: pixelAddress.cpp
 *
 *     Description: All pixelAddress controls.
 */

#include "captureOEMLite.h"

using namespace std;

PxLPixelAddress::PxLPixelAddress (GtkWidget *modeCombo, GtkWidget *valueCombo)
: m_modeCombo(modeCombo)
, m_valueCombo(valueCombo)
{
}

PxLPixelAddress::~PxLPixelAddress ()
{
}

// Disables all exposure controls/
void PxLPixelAddress:: greyAll()
{
    gtk_combo_box_text_remove_all (GTK_COMBO_BOX_TEXT(m_modeCombo));
	gtk_widget_set_sensitive (m_modeCombo, false);
    gtk_combo_box_text_remove_all (GTK_COMBO_BOX_TEXT(m_valueCombo));
    gtk_widget_set_sensitive (m_valueCombo, false);
}

// enables all supported pixel address controls, and initializes them to current values.
void PxLPixelAddress:: initialize()
{
    PxLAutoLock lock(&pCameraLock);

    //
    // Step 1
    //      Start with empty pixel address controls.  Only attempt to populate
    //      them if we have a camera that supports pixel addressing
    m_modeComboEntries.clear();
    m_valueComboEntries.clear();
    if (pCamera)
    {
        if (pCamera->supported(FEATURE_PIXEL_ADDRESSING))
        {
            float minMode, maxMode, currentMode;
            float minValue, maxValue, currentValue;
            bool restoreRequired = false;
            PXL_RETURN_CODE rc;

            //
            // Step 2
            //      we have a camera that supports pixel addressing.  Find the range
            //      of modes and values supported
            gtk_widget_set_sensitive (m_modeCombo, true);
            gtk_widget_set_sensitive (m_valueCombo, true);

            pCamera->getPixelAddressRange(&minMode, &maxMode, &minValue, &maxValue);
            pCamera->getPixelAddressValue(&currentMode, &currentValue);

            //
            // Step 3
            //      We know the camera support the 'min' pixel pixel address mode and value
            gtk_combo_box_text_insert_text (GTK_COMBO_BOX_TEXT(m_modeCombo),
                                            modeFromApi(minMode),
                                            PxLAddressModes[modeFromApi(minMode)]);
            m_modeComboEntries.push_back(modeFromApi(minMode));
            gtk_combo_box_text_insert_text (GTK_COMBO_BOX_TEXT(m_valueCombo),
                                            valueFromApi(minValue),
                                            PxLAddressValues[valueFromApi(minValue)]);
            m_valueComboEntries.push_back(valueFromApi(minValue));

            //
            // Step 4.
            //      The pixel address values and modes between the min and max are a little tougher.
            //      The camera may not support all values in the range.  The only way to see
            //      which of the ones the middle that are supported, is to try them.

            // Leave the camera in it's current mode, and try all of the values
            for (float candidate = minValue+1.0; candidate < maxValue; candidate+=1.0)
            {
                rc = pCamera->setPixelAddressValue(currentMode, candidate);
                if (API_SUCCESS(rc)) {
                    if (candidate != currentValue)restoreRequired = true;
                    gtk_combo_box_text_insert_text (GTK_COMBO_BOX_TEXT(m_valueCombo),
                                                    valueFromApi(candidate),
                                                    PxLAddressValues[valueFromApi(candidate)]);
                    m_valueComboEntries.push_back(valueFromApi(candidate));
                }
            }

            // Now, use the maximum value, try all of the modes
            for (float candidate = minMode+1.0; candidate < maxMode; candidate+=1.0)
            {
                rc = pCamera->setPixelAddressValue(candidate, maxValue);
                if (API_SUCCESS(rc)) {
                    if (candidate != currentMode)restoreRequired = true;
                    gtk_combo_box_text_insert_text (GTK_COMBO_BOX_TEXT(m_modeCombo),
                                                    modeFromApi(candidate),
                                                    PxLAddressModes[modeFromApi(candidate)]);
                    m_modeComboEntries.push_back(modeFromApi(candidate));
                }
            }

            //
            // Step 5.
            //      If we changed the pixel address value/mode in step 4, then restore the
            //      restore the old pixel addressing.
            if (restoreRequired) pCamera->setPixelAddressValue(currentMode, currentValue);

            // Step 6.
            //      We know the camera support the 'max' pixel address mode/value
            if (maxMode > minMode)
            {
                gtk_combo_box_text_insert_text (GTK_COMBO_BOX_TEXT(m_modeCombo),
                                                modeFromApi(maxMode),
                                                PxLAddressModes[modeFromApi(maxMode)]);
                m_modeComboEntries.push_back(modeFromApi(maxMode));
            }
            if (maxValue > minValue)
            {
                gtk_combo_box_text_insert_text (GTK_COMBO_BOX_TEXT(m_valueCombo),
                                                valueFromApi(maxValue),
                                                PxLAddressValues[valueFromApi(maxValue)]);
                m_valueComboEntries.push_back(valueFromApi(maxValue));
            }

            //
            // Step 7.
            //      And finally, show our current pixel address as the 'active' one
            for (int i=0; i<(int)m_modeComboEntries.size(); i++)
            {
                if (m_modeComboEntries[i] != modeFromApi(currentMode)) continue;
                gtk_combo_box_set_active (GTK_COMBO_BOX(m_modeCombo),i);
                break;
            }
            for (int i=0; i<(int)m_valueComboEntries.size(); i++)
            {
                if (m_valueComboEntries[i] != valueFromApi(currentValue)) continue;
                gtk_combo_box_set_active (GTK_COMBO_BOX(m_valueCombo),i);
                break;
            }
        }
    }
}

extern "C" void NewPixelAddressModeSelected
    (GtkWidget* widget, GdkEventExpose* event, gpointer userdata )
{
    if (!pCamera) return;

    gint modeIndex = gtk_combo_box_get_active (GTK_COMBO_BOX(pixelAddressObject->m_modeCombo));
    gint valueIndex = gtk_combo_box_get_active (GTK_COMBO_BOX(pixelAddressObject->m_valueCombo));

    // Don't bother to set the new mode, if the current value is PA_NONE, or negative (uninitialized)
    if (modeIndex < 0 || valueIndex<0) return;
    if (PA_NONE == pixelAddressObject->m_valueComboEntries[valueIndex]) return;

    pCamera->setPixelAddressValue(
            pixelAddressObject->modeToApi(pixelAddressObject->m_modeComboEntries[modeIndex]),
            pixelAddressObject->valueToApi(pixelAddressObject->m_valueComboEntries[valueIndex]));

    // Update our frame rate control, as it's limit have probably changed.
    frameRateObject->updateFrameRate();
}

extern "C" void NewPixelAddressValueSelected
    (GtkWidget* widget, GdkEventExpose* event, gpointer userdata )
{
    if (!pCamera) return;

    gint modeIndex = gtk_combo_box_get_active (GTK_COMBO_BOX(pixelAddressObject->m_modeCombo));
    gint valueIndex = gtk_combo_box_get_active (GTK_COMBO_BOX(pixelAddressObject->m_valueCombo));

    // Don't bother to set the new mode, if the current value is negative (uninitialized)
    if (modeIndex < 0 || valueIndex<0) return;

    pCamera->setPixelAddressValue(
            pixelAddressObject->modeToApi(pixelAddressObject->m_modeComboEntries[modeIndex]),
            pixelAddressObject->valueToApi(pixelAddressObject->m_valueComboEntries[valueIndex]));

    // Update our frame rate control, as it's limit have probably changed.
    frameRateObject->updateFrameRate();
}
