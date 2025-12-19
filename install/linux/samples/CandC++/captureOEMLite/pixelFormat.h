
/***************************************************************************
 *
 *     File: pixelFormat.h
 *
 *     Description: Simple wrapper class for all of the pixel format controls
 *
 */

#if !defined(PIXELINK_PIXEL_FORMAT_H)
#define PIXELINK_PIXEL_FORMAT_H

#include "PixeLINKApi.h"
#include <gtk/gtk.h>
#include <vector>

typedef enum _PXL_PIXEL_FORMATS
{
   MONO8 = 0,
   MONO16,
   MONO12_PACKED,
   BAYER8,
   BAYER16,
   BAYER12_PACKED,
   YUV422
} PXL_PIXEL_FORMATS;

static const char * const PxLFormats[] =
{
   "Mono8",
   "Mono16",
   "Mono12 Packed",
   "Bayer8",
   "Bayer16",
   "Bayer12 Packed",
   "YUV422"
};

class PxLPixelFormat
{
public:
    // Constructor
	PxLPixelFormat (GtkWidget *combo);
	// Destructor
	~PxLPixelFormat ();

    void  greyAll ();
    void  initialize ();

    GtkWidget    *m_combo;
    std::vector<PXL_PIXEL_FORMATS> m_comboEntries;

    // simple routines to convert pixel format to/from the ones used by the PixeLINK API
    PXL_PIXEL_FORMATS fromApi (float apiPixelFormat);
    float             toApi (PXL_PIXEL_FORMATS pixelFormat);

};

inline PXL_PIXEL_FORMATS PxLPixelFormat::fromApi(float apiPixelFormat)
{
    switch ((int)apiPixelFormat)
    {
    case PIXEL_FORMAT_MONO8:
        return MONO8;
    case PIXEL_FORMAT_MONO16:
        return MONO16;
    case PIXEL_FORMAT_MONO12_PACKED_MSFIRST:
        return MONO12_PACKED;
    case PIXEL_FORMAT_BAYER8:
    case PIXEL_FORMAT_BAYER8_RGGB:
    case PIXEL_FORMAT_BAYER8_GBRG:
    case PIXEL_FORMAT_BAYER8_BGGR:
        return BAYER8;
    case PIXEL_FORMAT_BAYER16:
    case PIXEL_FORMAT_BAYER16_RGGB:
    case PIXEL_FORMAT_BAYER16_GBRG:
    case PIXEL_FORMAT_BAYER16_BGGR:
        return BAYER16;
    case PIXEL_FORMAT_BAYER12_PACKED_MSFIRST:
    case PIXEL_FORMAT_BAYER12_RGGB_PACKED_MSFIRST:
    case PIXEL_FORMAT_BAYER12_GBRG_PACKED_MSFIRST:
    case PIXEL_FORMAT_BAYER12_BGGR_PACKED_MSFIRST:
        return BAYER12_PACKED;
    case PIXEL_FORMAT_YUV422:
        return YUV422;
    default:
        return MONO8;
    }
}

inline float PxLPixelFormat::toApi (PXL_PIXEL_FORMATS pixelFormat)
{
    switch (pixelFormat)
    {
    case MONO8:          return (float) PIXEL_FORMAT_MONO8;
    case MONO16:         return (float) PIXEL_FORMAT_MONO16;
    case MONO12_PACKED:  return (float) PIXEL_FORMAT_MONO12_PACKED_MSFIRST;
    case BAYER8:         return (float) PIXEL_FORMAT_BAYER8;
    case BAYER16:        return (float) PIXEL_FORMAT_BAYER16;
    case BAYER12_PACKED: return (float) PIXEL_FORMAT_BAYER12_PACKED_MSFIRST;
    case YUV422:         return (float) PIXEL_FORMAT_YUV422;
    default:             return (float)PIXEL_FORMAT_MONO8;
    }
}

#endif // !defined(PIXELINK_PIXEL_FORMAT_H)
