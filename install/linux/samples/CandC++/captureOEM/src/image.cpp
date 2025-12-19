/***************************************************************************
 *
 *     File: image.cpp
 *
 *     Description:
 *        Controls for the 'Image' tab  in CaptureOEM.
 */

#include <glib.h>
#include <string>
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include "image.h"
#include "camera.h"
#include "captureOEM.h"
#include "helpers.h"
#include "cameraSelect.h"

using namespace std;

// Allow the user to save an image as the raw data from the camera.
#define IMAGE_FORMAT_RAW  IMAGE_FORMAT_PNG + 1


extern PxLImage      *gImageTab;

//
// Local prototypes.
//    UI updates can only be done from a gtk thread -- these routines are gtk 'idle' threads
//    and as such are 'UI update safe'. For each 'feature', there there the following functions:
//       . {featureXXX}Deactivate - Makes the controls meaningless (including greying them out)
//       . {featreuXXX}Activate - updates the controls with values from the camera
static gboolean  RefreshComplete (gpointer pData);
static gboolean  ImageDeactivate (gpointer pData);
static gboolean  ImageActivate (gpointer pData);

/* ---------------------------------------------------------------------------
 * --   Member functions - Public
 * ---------------------------------------------------------------------------
 */
PxLImage::PxLImage (GtkBuilder *builder)
{
    //
    // Step 1
    //      Find all of the glade controls

    m_fileName = GTK_WIDGET( gtk_builder_get_object( builder, "ImageName_Tex" ) );
    m_fileType = GTK_WIDGET( gtk_builder_get_object( builder, "ImageType_Combo" ) );
    m_fileLocation = GTK_WIDGET( gtk_builder_get_object( builder, "ImageLocation_Text" ) );
    m_fileLocationBrowser = GTK_WIDGET( gtk_builder_get_object( builder, "ImageLocation_Button" ) );
    m_fileNameIncrement = GTK_WIDGET( gtk_builder_get_object( builder, "ImageNameIncrement_Checkbutton" ) );
    m_captureLaunch = GTK_WIDGET( gtk_builder_get_object( builder, "ImageLaunch_Checkbutton" ) );
    m_captureButton = GTK_WIDGET( gtk_builder_get_object( builder, "ImageCapture_Button" ) );

    //
    // Step 2
    //      Initialize our image defaults
    gtk_entry_set_text (GTK_ENTRY(m_fileName), "image.bmp");

    gtk_combo_box_text_insert_text (GTK_COMBO_BOX_TEXT(m_fileType),
                                    IMAGE_BITMAP,
                                    "Bitmap");
    gtk_combo_box_text_insert_text (GTK_COMBO_BOX_TEXT(m_fileType),
                                    IMAGE_JPEG,
                                    "JPEG");
    gtk_combo_box_text_insert_text (GTK_COMBO_BOX_TEXT(m_fileType),
                                    IMAGE_PNG,
                                    "PNG");
    gtk_combo_box_text_insert_text (GTK_COMBO_BOX_TEXT(m_fileType),
                                    IMAGE_TIFF,
                                    "TIFF");
    gtk_combo_box_text_insert_text (GTK_COMBO_BOX_TEXT(m_fileType),
                                    IMAGE_ADOBE,
                                    "Adobe Photoshop");
    gtk_combo_box_text_insert_text (GTK_COMBO_BOX_TEXT(m_fileType),
                                    IMAGE_RAW,
                                    "Raw Data");
    gtk_combo_box_set_active (GTK_COMBO_BOX(m_fileType),IMAGE_BITMAP);

    gchar* picturesDir = g_strdup_printf ("%s/Pictures", g_get_home_dir());
    gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER(m_fileLocationBrowser),
                                        picturesDir);
    g_free(picturesDir);

    gtk_entry_set_text (GTK_ENTRY(m_fileLocation),
                        gtk_file_chooser_get_current_folder (GTK_FILE_CHOOSER(m_fileLocationBrowser)));

}


PxLImage::~PxLImage ()
{
}

void PxLImage::refreshRequired (bool noCamera)
{
    if (IsActiveTab (ImageTab))
    {
        if (noCamera)
        {
            // If I am the active tab, then grey out everything
            gdk_threads_add_idle ((GSourceFunc)ImageDeactivate, this);
        } else {
            // If I am the active tab, then refresh everything
            gdk_threads_add_idle ((GSourceFunc)ImageActivate, this);
        }

        gdk_threads_add_idle ((GSourceFunc)RefreshComplete, this);
        m_numRefreshRequestsOutstanding++;
    } else {
        // If we are not the active tab, only bump the m_numRefreshRequestsOutstanding if there is not
        // one outstanding already; RefreshComplete will be scheduled when the tab becomes active
        if (!m_numRefreshRequestsOutstanding)m_numRefreshRequestsOutstanding++;
    }
}

void PxLImage::activate()
{
    // I have become the active tab.

    if (gCamera)
    {
        if (m_numRefreshRequestsOutstanding)
        {
            gdk_threads_add_idle ((GSourceFunc)ImageActivate, this);
        }
    } else {
        gdk_threads_add_idle ((GSourceFunc)ImageDeactivate, this);
    }

    m_numRefreshRequestsOutstanding = 1; // As a safety mechanism, tab activation should assert value, it will be set to 0 when RefreshComplete
    gdk_threads_add_idle ((GSourceFunc)RefreshComplete, this);
}

void PxLImage::deactivate()
{
    // I am no longer the active tab.
}


/* ---------------------------------------------------------------------------
 * --   gtk thread callbacks - used to update controls
 * ---------------------------------------------------------------------------
 */

// Indicate that the refresh is no longer outstanding, it has completed.
static gboolean RefreshComplete (gpointer pData)
{
    PxLImage *pControls = (PxLImage *)pData;

    pControls->m_numRefreshRequestsOutstanding--;
    return false;
}

//
// Make image controls unselectable
static gboolean ImageDeactivate (gpointer pData)
{
    PxLImage *pControls = (PxLImage *)pData;

    // The only control that is ever deactivated, is the capture button

    gtk_widget_set_sensitive (pControls->m_captureButton, false);

    return false;  //  Only run once....
}

//
// Make image controls selectable (if appropriate)
static gboolean ImageActivate (gpointer pData)
{
    PxLImage *pControls = (PxLImage *)pData;

    if (gCamera)
    {
        gtk_widget_set_sensitive (pControls->m_captureButton, ! gCamera->hardwareTriggering());
    }

    return false;  //  Only run once....
}

/* ---------------------------------------------------------------------------
 * --   Control functions from the Glade project
 * ---------------------------------------------------------------------------
 */

extern "C" void NewImageFormatSelected
    (GtkWidget* widget, GdkEventExpose* event, gpointer userdata )
{
    if (! gImageTab) return;
    if (gImageTab->m_numRefreshRequestsOutstanding) return;

    PxLImage::IMAGE_FORMATS imageFormatIndex = (PxLImage::IMAGE_FORMATS)gtk_combo_box_get_active (GTK_COMBO_BOX(gImageTab->m_fileType));

    switch (imageFormatIndex)
    {
    case PxLImage::IMAGE_BITMAP:
    default:
        ReplaceFileExtension (GTK_ENTRY(gImageTab->m_fileName), "bmp");
        break;
    case PxLImage::IMAGE_JPEG:
        ReplaceFileExtension (GTK_ENTRY(gImageTab->m_fileName), "jpeg");
        break;
    case PxLImage::IMAGE_PNG:
        ReplaceFileExtension (GTK_ENTRY(gImageTab->m_fileName), "png");
        break;
    case PxLImage::IMAGE_TIFF:
        ReplaceFileExtension (GTK_ENTRY(gImageTab->m_fileName), "tiff");
        break;
    case PxLImage::IMAGE_ADOBE:
        ReplaceFileExtension (GTK_ENTRY(gImageTab->m_fileName), "psd");
        break;
    case PxLImage::IMAGE_RAW:
        ReplaceFileExtension (GTK_ENTRY(gImageTab->m_fileName), "raw");
        break;
    }
}

extern "C" void NewImageLocation
    (GtkWidget* widget, GdkEventExpose* event, gpointer userdata )
{
    if (! gImageTab) return;
    if (gImageTab->m_numRefreshRequestsOutstanding) return;

    gchar* newFolder = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER(gImageTab->m_fileLocationBrowser));

    gtk_entry_set_text (GTK_ENTRY(gImageTab->m_fileLocation), newFolder);
    g_free(newFolder);
}

extern "C" void ImageIncrementToggled
    (GtkWidget* widget, GdkEventExpose* event, gpointer userdata )
{
    if (! gImageTab) return;
    if (gImageTab->m_numRefreshRequestsOutstanding) return;

    if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON(gImageTab->m_fileNameIncrement)))
    {
        IncrementFileName (GTK_ENTRY(gImageTab->m_fileName), "%04d");
    }

}

extern "C" void ImageCaptureButtonPressed
    (GtkWidget* widget, GdkEventExpose* event, gpointer userdata )
{

    if (! gImageTab) return;
    if (! gCamera) return;
    if (gImageTab->m_numRefreshRequestsOutstanding) return;

    //
    // Step 1
    //      Start the stream (if it's not already running
    PXL_RETURN_CODE rc = ApiSuccess;
    bool wasStreaming = gCamera->streaming();
    if (!wasStreaming) rc = gCamera->startStream();
    if (!API_SUCCESS (rc)) return;

    //
    // Step 2
    //      Grab an image
    std::vector<U8> frameBuf (gCamera->imageSizeInBytes());
    FRAME_DESC     frameDesc;

    rc = gCamera->getNextFrame (frameBuf.size(), &frameBuf[0], &frameDesc);
    if (API_SUCCESS(rc))
    {
        PxLImage::IMAGE_FORMATS format = (PxLImage::IMAGE_FORMATS)gtk_combo_box_get_active (GTK_COMBO_BOX(gImageTab->m_fileType));
        //
        // Step 3
        //      All image formats other than raw, require an intermediate buffer
        U8* imageToWrite;
        std::vector<U8> bBuffer;
        ULONG bytesToWrite = 0;
        if (format != PxLImage::IMAGE_RAW)
        {
            //
            // Step 4
            //      Format the image
            rc = gCamera->formatImage(&frameBuf[0], &frameDesc, gImageTab->toApiImageFormat(format), &bytesToWrite, NULL);
            bBuffer.resize(bytesToWrite);
            rc = gCamera->formatImage(&frameBuf[0], &frameDesc, gImageTab->toApiImageFormat(format), &bytesToWrite, &bBuffer[0]);
            if (! API_SUCCESS(rc)) return;
            imageToWrite = &bBuffer[0];
        } else {
            imageToWrite = &frameBuf[0];
            bytesToWrite = frameBuf.size();
        }

        //
        // Step 5
        //      Save the image to file
        string fileName = gtk_entry_get_text (GTK_ENTRY(gImageTab->m_fileLocation));
        fileName = fileName + '/';
        fileName = fileName + gtk_entry_get_text (GTK_ENTRY(gImageTab->m_fileName));
        FILE* pFile;

        // Open our file for binary write
        size_t numBytesWritten;
        pFile = fopen(fileName.c_str(), "wb");
        if (NULL == pFile) return;

        numBytesWritten = fwrite(imageToWrite, sizeof(char), bytesToWrite, pFile);
        printf ("Write %d bytes to file %s\n", (int)numBytesWritten, fileName.c_str());

        fclose(pFile);

        //
        // Step 6
        //      Bump up the file name if we need to
        if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON(gImageTab->m_fileNameIncrement)))
        {
            IncrementFileName (GTK_ENTRY(gImageTab->m_fileName), "%04d");
        }

        //
        // Step 7
        //      Launch a viewer if the user requested it.
        if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON(gImageTab->m_captureLaunch)))
        {
            pid_t pid = fork();

            if (pid == 0)
            {
                execl ("/usr/bin/xdg-open", "xdg-open", fileName.c_str(), NULL);
            }
        }

    }

    //
    // Step 8
    //      Stop the stream if we started it.
    if (!wasStreaming) gCamera->stopStream();

}



