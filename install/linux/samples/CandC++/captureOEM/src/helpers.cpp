/***************************************************************************
 *
 *     File: image.cpp
 *
 *     Description:
 *        Controls for the 'Image' tab  in CaptureOEM.
 */

#include <string>
#include <stdio.h>
#include <stdlib.h>
#include <glib.h>
#include <vector>
#include <algorithm>
#include "helpers.h"
#include "camera.h"
#include "PixeLINKApi.h"

using namespace std;

// This msut be the first 4 bytes of a PxL Camera Config file.
// This is the same one used by capture OEM on Windows
#define PXL_CONFIG_FILE_MAGIC_NUMBER 0x41513879

// Struct used only to read and write features to file.
struct FeatureData
{
    U32     featureId;
    U32     flags;
    U32     nParams;
    float   param1;
};


/**
* Function: IncrementFileName
* Purpose:  Read the file name from the provided window, parse out whatever
*           number immediately precedes the file extension, increment the
*           number by one, and change the file name in the control to contain
*           the new number.
*           Eg: Changes "C:\image_3.bmp" to"C:\image_4.bmp".
*/
void IncrementFileName(GtkEntry *entry, const char* format)
{
    //
    // Step 1
    //      Find where the '.' is in the file name (if any)
    string name = gtk_entry_get_text (entry);
    size_t dotPos = name.rfind('.');
    if (dotPos == string::npos) dotPos = name.length();

    //
    // Step 2
    //      Find the number immediately preceeding the '.' (if there is one)
    size_t numPos = name.find_last_not_of ("0123456789", dotPos-1);
    if (numPos == string::npos)numPos = dotPos;
    else numPos++;
    int fileNum;
    if (numPos == dotPos)
    {
        // there is no current fileNum, so, start with 1
        fileNum = 1;
    } else {
        fileNum = (atoi ((name.substr (numPos, dotPos-numPos)).c_str())) + 1;
    }

    //
    // Step 3
    //      Build a new string using the 3 components
    gchar* newFileNum = g_strdup_printf (format, fileNum);
    string newName = name.substr (0, numPos) + newFileNum + name.substr (dotPos, name.length() - dotPos);
    g_free (newFileNum);

    //
    // Step 4
    //      Put this new name into the control
    gtk_entry_set_text (entry, newName.c_str());

}

/**
* Function: IncrementFileName
* Purpose:  Read the file name from the provided window, parse the file name,
*           replacing the extension with the requested one.
*           Eg: Changes "image_3.bmp" to"image_3.jpeg".
*/
void ReplaceFileExtension(GtkEntry *entry, const char* newExtension)
{
    //
    // Step 1
    //      Find where the '.' is in the file name (if any)
    string name = gtk_entry_get_text (entry);
    size_t dotPos = name.rfind('.');
    if (dotPos == string::npos) dotPos = name.length();


    //
    // Step 2
    //      Build a new string using the 2 components
    name.resize (dotPos);
    name = name + '.' + newExtension;

    //
    // Step 3
    //      Put this new name into the control
    gtk_entry_set_text (entry, name.c_str());

}

/**
* Function: WriteConfigFile
* Purpose:  Queries the cameras for all of it's supported features, and
*           records those values to a file.  Note that file uses the forma
*           as does capture OEM for Windows.  Specifically....
    File Format of config files:
    int Magic Number
    int #descriptors, which is always 0 (not supported) for Linux based cameras
    [feature data starts here - either for desc #1, or just the normal settings if #descriptors == 0]
    [int desc #1 update mode, if #descriptors > 0]
    int #features (=N == number of supported features)
        feature #1 : consists of a FeatureData struct, plus [nParams-1] extra floats immediately following the struct.
        feature #2 : ditto
        etc
        feature #N
    [descriptor#2 starts here]
    int desc #2 update mode
    int #features (=M == number of desc-supported features
        feature #1 : as above
        etc
        feature #M
    [descriptor #3 starts here]
        etc
    [tab 1 starts here]
        // Note that it's optional for a tab to save its data; most don't
        // 'tab 1' doesn't mean the first tab in Capture OEM, but rather the first tab to actually save data
        int tab IDD
        # bytes of data
        [tab 1-specific data starts here]
        tab-specific data
    [tab 2 starts here]
        etc
    etc
    [tab N starts here]
    END OF FILE
*
*/
bool WriteConfigFile(PxLCamera* camera,const gchar* fileName)
{
    //
    // Step 1
    //      Determine how much memory we need to allocate for the data.
    //      Find out how many features are supported and how many parameters
    //      they have.
    int nSupported = 0;
    int nExtraParams = 0;
    int featureId;
    PXL_RETURN_CODE rc;
    int numGpios =0;

    for (featureId = 0; featureId < FEATURES_TOTAL; ++featureId)
    {
        if (!camera->supported(featureId))
            continue;
        if (featureId == FEATURE_MEMORY_CHANNEL) continue;  // Don't save or restore this feature

        int num = 1;
        if (featureId == FEATURE_GPIO)
        {
            // GPIO is special, we will record a FeatureData for each GPIO
            float minMode, maxMode;
            rc = camera->getGpioRange(&numGpios, &minMode, &maxMode);
            if (rc == ApiSuccess) num = numGpios;
        }
        nSupported += num;

        U32 pcount = camera->numParametersSupported(featureId);
        nExtraParams += num * (pcount - 1); // First param is inside the FeatureDesc struct - we just need to count *extra* params.
    }

    //
    // Step 2.
    //      Allocate the buffer
    int bufsize = 3 * sizeof(int) // Magic number, #descriptors, and feature count
                    + nSupported * sizeof(FeatureData)
                    + nExtraParams * sizeof(float);
    vector<U8> buffer(bufsize); // buffer should now be exactly the right size.
    // allow room for growth without having to realloc
    buffer.reserve(10*1024);            // NOTE: this also gets rid of a bug in the debug std::vector dtor

    //
    // Step 3.
    //      Set the initial 'header' elements
    int* ip = reinterpret_cast<int*>(&buffer[0]);
    *ip++ = PXL_CONFIG_FILE_MAGIC_NUMBER;
    *ip++ = 0;  // No descriptors ---- Linux cameras do not support descriptors
    *ip++ = nSupported;  // Number of features

    //
    // Step 4.
    //      Read and record all supported features.
    U8* pbuf = reinterpret_cast<U8*>(ip);
    // 2004-06-29 - Now loops from highest to lowest featureID, to work around
    //              obscure bug in 682/782 (essentially, we need to set Lookup
    //              Table before Gamma when we Import config files).
    for (featureId = FEATURES_TOTAL-1; featureId >= 0; --featureId)
    {
        if (!camera->supported(featureId))
            continue;
        if (featureId == FEATURE_MEMORY_CHANNEL) continue;  // Don't save or restore this feature

        U32 pcount = camera->numParametersSupported(featureId);

        if (featureId == FEATURE_GPIO)
        {
            // GPIO is special - there may be more than one GPO supported, but
            // we can only read them one at a time. We need to read each of them
            // in a loop.
            for (int gpioNum = 1; gpioNum <= numGpios; ++gpioNum)
            {
                FeatureData* pfd = reinterpret_cast<FeatureData*>(pbuf);
                pfd->featureId = featureId;
                pfd->nParams = pcount;
                pfd->param1 = static_cast<float>(gpioNum);
                rc = PxLGetFeature( camera->getHandle(),
                                    featureId,
                                    &pfd->flags,
                                    &pfd->nParams,
                                    &pfd->param1  );
                pbuf += sizeof(FeatureData) + (pcount-1) * sizeof(float);
            }
        }
        else
        {
            // All other features (other than GPIO) can be read in one go.
            FeatureData* pfd = reinterpret_cast<FeatureData*>(pbuf);
            pfd->featureId = featureId;
            pfd->nParams = pcount;

            rc = PxLGetFeature(  camera->getHandle(),
                                 featureId,
                                 &pfd->flags,
                                 &pfd->nParams,
                                 &pfd->param1  );

            pbuf += sizeof(FeatureData) + (pcount-1) * sizeof(float);
        }
    }

    // Step 5.
    //      Sanity check
    // At this point, we should have filled the vector with exactly the number
    // of bytes that we pre-calculated, so the pointer should point one byte
    // past the end of the vector.
    {
        U8 const * const pEnd = &buffer[buffer.size()-1] + 1;
        ASSERT(pbuf == pEnd);
    }

    //
    // Step 6.
    //      Save the data to disk
    FILE* configFile;
    configFile = fopen(fileName, "wb");
    if(NULL == configFile) return false;

    size_t numBytesWritten = fwrite(&buffer[0], sizeof(U8), (U32)buffer.size(), configFile);

    fclose(configFile);

    return (numBytesWritten == buffer.size());  // return true only if the entire buffer was written
}

bool ReadConfigFile(PxLCamera* camera,const gchar* fileName)
{
    //
    // Step 1
    //      Determine how much memory we need to allocate for the data.
    FILE* configFile;
    configFile = fopen(fileName, "rb");
    if(NULL == configFile) return false;

    fseek (configFile, 0, SEEK_END); // Jump to the end so we can get teh file size
    int fileSize = ftell (configFile);
    if (fileSize <= 0)
    {
        fclose (configFile);
        return false;
    }
    fseek (configFile, 0, SEEK_SET); // Jump back the beginning of the file again

    //
    // Step 2
    //      Read the file into an allocated buffer
    std::vector<U8> buffer(fileSize,0);

    size_t numBytesRead = fread(&buffer[0], sizeof(U8), (U32)buffer.size(), configFile);
    fclose (configFile); // We are done with this, close it
    if (numBytesRead != buffer.size()) return false;

    //
    // Step 3
    //      Read how many features are represented in the file
    U8* pbuf = &buffer[0];
    int* ip = reinterpret_cast<int*>(pbuf);
    if (*ip++ != PXL_CONFIG_FILE_MAGIC_NUMBER) return false; // This is NOT a configuration file
    if (*ip++ != 0) return false;  // This condifuration file camera from a camera with multiple descriptors -- we don't support that
    int nFeatures = *ip++; // # of features
    pbuf = reinterpret_cast<U8*>(ip);

    //
    // Step 4
    //      Set each camera feature according to the feature value from the config file
    for (int i = 0; i < nFeatures; i++)
    {
        FeatureData* pfd = reinterpret_cast<FeatureData*>(pbuf);

        U32 cameraNParams = camera->numParametersSupported(pfd->featureId);
        if (pfd->featureId != FEATURE_MEMORY_CHANNEL && // Don't try to set the memory channel
            camera->manualSupported(pfd->featureId))
        {
             // If the number of parameters do nt match, use the lesser of the. If the camera
             // actually sports more, it should be able to accommodate.
             PXL_RETURN_CODE rc =
                 PxLSetFeature( camera->getHandle(),
                                pfd->featureId,
                                pfd->flags,
                                pfd->nParams < cameraNParams ? pfd->nParams: cameraNParams,
                                &pfd->param1);
         }

         pbuf += sizeof(FeatureData) + (pfd->nParams - 1) * sizeof(float);
    }

    return true;
}

// See PxLComboBox comments in helpers.h
PxLComboBox::PxLComboBox(GtkWidget* gtkComboBox)
{
    m_gtkComboBox = gtkComboBox;
}

void PxLComboBox::setSensitive (bool bSensitive)
{
    gtk_widget_set_sensitive (m_gtkComboBox, bSensitive);
}

void PxLComboBox::addItem (int itemValue, const char* itemText)
{
    // We need to slot this item into the sorted list
    int pos = 0;
    for (vector<int>::iterator it = m_values.begin(); it != m_values.end(); ++it, pos++)
    {
        if (*it < itemValue) continue;
        if (*it == itemValue) return;   // Don't allow the item to appear more than once
        // The next value must have a larger value -- insert the new one here
        m_values.insert (it, itemValue);
        gtk_combo_box_text_insert_text (GTK_COMBO_BOX_TEXT(m_gtkComboBox),
                                        pos,
                                        itemText);
        return;
    }

    // If we made it this far, the new item goes at the end.
    m_values.push_back (itemValue);

    gtk_combo_box_text_insert_text (GTK_COMBO_BOX_TEXT(m_gtkComboBox),
                                    pos,
                                    itemText);
}

void PxLComboBox::removeItem (int itemValue)
{
    int pos = 0;
    vector<int>::iterator it = m_values.begin();
    for (; it != m_values.end(); ++it, pos++)
    {
        if (*it == itemValue) 
        {
            gtk_combo_box_text_remove (GTK_COMBO_BOX_TEXT(m_gtkComboBox), pos);
            break;
        }
    }
    if (it != m_values.end()) m_values.erase(it);
}

void PxLComboBox::removeAll ()
{
    gtk_combo_box_text_remove_all (GTK_COMBO_BOX_TEXT(m_gtkComboBox));
    m_values.clear();
}

void PxLComboBox::makeActive (int itemValue)
{
    int pos = 0;
    for (vector<int>::iterator it = m_values.begin(); it != m_values.end(); ++it, pos++)
    {
        if (*it == itemValue) 
        {
            gtk_combo_box_set_active (GTK_COMBO_BOX(m_gtkComboBox), pos);
            break;
        }
    }
}

int PxLComboBox::getSelectedItem()
{
    return m_values[gtk_combo_box_get_active (GTK_COMBO_BOX(m_gtkComboBox))];
}


