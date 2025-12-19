
/***************************************************************************
 *
 *     File: imageCapture.h
 *
 *     Description: Simple wrapper class for all of the image controls
 *
 *     Design Notes:
 *       - As a 'simplification', the image formats presented in the glade project (and used
 *         in this module), are sequenced using the same enumerations as the IMAGE_FORMAT_XXXX
 *         defined in PixeLINKTypes.h.
 */

#if !defined(PIXELINK_IMAGE_CAPTURE_H)
#define PIXELINK_IMAGE_CAPTURE_H

#include "PixeLINKApi.h"
#include <gtk/gtk.h>
#include <vector>

// IMAGE_FORMAT_RAW is not in PixeLINKTypes.h -- define it to be the one after the last one
// used by this software:
#define IMAGE_FORMAT_RAW (IMAGE_FORMAT_JPEG+1)

// as per the Design Note at the top of this module, this must be in the same order as glade project
// and PixeLINKTypes.h
static const char * const PxLImageFormatTypes[] =
{
   ".bmp",   // IMAGE_FORMAT_BMP
   ".tiff",  // IMAGE_FORMAT_TIFF
   ".psd",   // IMAGE_FORAMT_PSD
   ".jpeg",  // IMAGE_FORAMT_JPEG
   ".bin"    // IMAGE_FORMAT_RAW
};

class PxLImageCapture
{
public:
    // Constructor
    PxLImageCapture (GtkWidget *fileName, GtkWidget *fileChooser, GtkWidget *fileType, GtkWidget *captureButton);

    void  greyAll ();
    void  enableCaptures (bool enable);

    GtkWidget    *m_fileName;
    GtkWidget    *m_fileChooser;
    GtkWidget    *m_fileType;
    GtkWidget    *m_captureButton;

};

#endif // !defined(PIXELINK_IMAGE_CAPTURE_H)
