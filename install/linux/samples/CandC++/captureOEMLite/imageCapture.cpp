
/***************************************************************************
 *
 *     File: imageCapture.cpp
 *
 *     Description: All image capture controls.
 */

#include "captureOEMLite.h"
#include <string.h>

using namespace std;

PxLImageCapture::PxLImageCapture (GtkWidget *fileName, GtkWidget *fileChooser, GtkWidget *fileType, GtkWidget *captureButton)
: m_fileName(fileName)
, m_fileChooser(fileChooser)
, m_fileType(fileType)
, m_captureButton(captureButton)
{
}

// Disables all exposure controls/
void PxLImageCapture:: greyAll()
{
    vector<char>defaultFile (FILENAME_MAX);

    // Use the current working directory as our 'default'
    if (getcwd(&defaultFile[0], FILENAME_MAX))
    {
        gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER(m_fileChooser), &defaultFile[0]);
        strcat (&defaultFile[0], "/image");
        strcat (&defaultFile[0], PxLImageFormatTypes[0]);
    } else {
        defaultFile[0] = 0;

    }

    gtk_widget_set_sensitive (m_fileName, false);
    gtk_widget_set_sensitive (m_fileChooser, false);
	gtk_widget_set_sensitive (m_fileType, false);
    gtk_widget_set_sensitive (m_captureButton, false);
    gtk_combo_box_set_active(GTK_COMBO_BOX(m_fileType), -1);
    gtk_entry_set_text (GTK_ENTRY (m_fileName), &defaultFile[0]);
}

// enables all supported pixel address controls, and initializes them to current values.
void PxLImageCapture:: enableCaptures(bool enable)
{
    gtk_widget_set_sensitive (m_fileName, enable);
    gtk_widget_set_sensitive (m_fileChooser, enable);
    gtk_widget_set_sensitive (m_fileType, enable);
    gtk_widget_set_sensitive (m_captureButton, enable);
    gtk_combo_box_set_active(GTK_COMBO_BOX(m_fileType), enable ? 0 : -1);
}

extern "C" void NewImageCaptureTypeSelected
    (GtkWidget* widget, GdkEventExpose* event, gpointer userdata )
{
    if (!pCamera) return;

    gint imageType = gtk_combo_box_get_active (GTK_COMBO_BOX(imageCaptureObject->m_fileType));
    if (imageType < 0) return;  // Don't bother if there is no image type (startup/shutdown)

    const char *controlFileName = gtk_entry_get_text (GTK_ENTRY (imageCaptureObject->m_fileName));

    // The control file name is read only, take a copy of it so that we can modify the name.
    vector<char>fullFileName (FILENAME_MAX);
    strncpy (&fullFileName[0], controlFileName, fullFileName.size());

    // If the file name has one of the extensions we use, then replace it with the new extension.  Otherwise,
    // leave it alone (as the user is controlling the complete file name).
    char* pExtension = strrchr (&fullFileName[0], '.');
    for (int i = IMAGE_FORMAT_BMP; i <= IMAGE_FORMAT_RAW; i++)
    {
        if (pExtension && ! strncmp (pExtension, PxLImageFormatTypes[i], strlen(PxLImageFormatTypes[i])))
        {
            // This is one of the extensions we know about.  Replace it with the new one.
            *pExtension = 0;
            strcat (&fullFileName[0], PxLImageFormatTypes[imageType]);
            gtk_entry_set_text (GTK_ENTRY (imageCaptureObject->m_fileName), &fullFileName[0]);
            break;
        }
    }

}

extern "C" void ImageCaptureButtonPressed
    (GtkWidget* widget, GdkEventExpose* event, gpointer userdata )
{
    if (!pCamera) return;

    gint imageType = gtk_combo_box_get_active (GTK_COMBO_BOX(imageCaptureObject->m_fileType));
    if (imageType < 0) return;  // Don't bother if there is no image type (startup/shutdown)

    pCamera->captureImage(gtk_entry_get_text (GTK_ENTRY (imageCaptureObject->m_fileName)), imageType);
}

extern "C" void FileSelectorButtonPressed
    (GtkWidget* widget, GdkEventExpose* event, gpointer userdata )
{
    if (!pCamera) return;

    gint imageType = gtk_combo_box_get_active (GTK_COMBO_BOX(imageCaptureObject->m_fileType));
    if (imageType < 0) return;  // Don't bother if there is no image type (startup/shutdown)

    // Build a new file name string based on the folder that the user selected.
    vector<char>fullFileName (FILENAME_MAX);
    strncpy (&fullFileName[0],
             gtk_file_chooser_get_filename (GTK_FILE_CHOOSER(imageCaptureObject->m_fileChooser)),
             fullFileName.size());

    strcat (&fullFileName[0], "/image");
    strcat (&fullFileName[0], PxLImageFormatTypes[imageType]);
    gtk_entry_set_text (GTK_ENTRY (imageCaptureObject->m_fileName), &fullFileName[0]);
}




