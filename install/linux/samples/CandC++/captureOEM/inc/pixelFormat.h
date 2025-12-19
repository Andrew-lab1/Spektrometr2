
/***************************************************************************
 *
 *     File: pixelFormat.h
 *
 *     Description: Simple wrapper class for all of the pixel format controls
 *
 */

#if !defined(PIXELINK_PIXEL_FORMAT_H)
#define PIXELINK_PIXEL_FORMAT_H

#include <assert.h>
#include "PixeLINKApi.h"

typedef enum _COEM_PIXEL_FORMATS
{
   MONO8 = 0,
   MONO16,
   MONO10_PACKED,
   MONO12_PACKED,
   BAYER8,
   BAYER16,
   BAYER10_PACKED,
   BAYER12_PACKED,
   YUV422,
   STOKES,
   POLAR,
   POLAR_RAW,
   HSV,
   RGB24,
   BGR24,
   RGBA,
   BGRA,
   ARGB,
   ABGR,
} COEM_PIXEL_FORMATS;

typedef enum _COEM_PIXEL_FORMAT_INTERPRETATIONS
{
   HSV_AS_COLOR = 0,
   HSV_AS_ANGLE,
   HSV_AS_DEGREE,
   NUM_COEM_PIXEL_FORMAT_INTERPRETATIONS
} COEM_PIXEL_FORMAT_INTERPRETATIONS;

class PxLPixelFormat
{
public:

    inline static COEM_PIXEL_FORMATS fromApi(float apiPixelFormat)
    {
        switch ((int)apiPixelFormat)
        {
        case PIXEL_FORMAT_MONO8:
            return MONO8;
        case PIXEL_FORMAT_MONO16:
            return MONO16;
        case PIXEL_FORMAT_MONO10_PACKED_MSFIRST:
            return MONO10_PACKED;
        case PIXEL_FORMAT_MONO12_PACKED:
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
        case PIXEL_FORMAT_BAYER10_PACKED_MSFIRST:
        case PIXEL_FORMAT_BAYER10_RGGB_PACKED_MSFIRST:
        case PIXEL_FORMAT_BAYER10_GBRG_PACKED_MSFIRST:
        case PIXEL_FORMAT_BAYER10_BGGR_PACKED_MSFIRST:
            return BAYER10_PACKED;
        case PIXEL_FORMAT_BAYER12_PACKED:
        case PIXEL_FORMAT_BAYER12_RGGB_PACKED:
        case PIXEL_FORMAT_BAYER12_GBRG_PACKED:
        case PIXEL_FORMAT_BAYER12_BGGR_PACKED:
        case PIXEL_FORMAT_BAYER12_PACKED_MSFIRST:
        case PIXEL_FORMAT_BAYER12_RGGB_PACKED_MSFIRST:
        case PIXEL_FORMAT_BAYER12_GBRG_PACKED_MSFIRST:
        case PIXEL_FORMAT_BAYER12_BGGR_PACKED_MSFIRST:
            return BAYER12_PACKED;
        case PIXEL_FORMAT_YUV422:
            return YUV422;
        case PIXEL_FORMAT_STOKES4_12:
            return STOKES;
        case PIXEL_FORMAT_POLAR4_12:
            return POLAR;
        case PIXEL_FORMAT_POLAR_RAW4_12:
            return POLAR_RAW;
        case PIXEL_FORMAT_HSV4_12:
            return HSV;
        case PIXEL_FORMAT_RGB24_NON_DIB:
            return RGB24;
        case PIXEL_FORMAT_BGR24_NON_DIB:
            return BGR24;
        case PIXEL_FORMAT_RGBA:
            return RGBA;
        case PIXEL_FORMAT_BGRA:
            return BGRA;
        case PIXEL_FORMAT_ARGB:
            return ARGB;
        case PIXEL_FORMAT_ABGR:
            return ABGR;

        default:
            return MONO8;
        }
    }

    inline static float toApi (COEM_PIXEL_FORMATS pixelFormat)
    {
        switch (pixelFormat)
        {
        case MONO8:          return (float) PIXEL_FORMAT_MONO8;
        case MONO16:         return (float) PIXEL_FORMAT_MONO16;
        case MONO10_PACKED:  return (float) PIXEL_FORMAT_MONO10_PACKED_MSFIRST;
        case MONO12_PACKED:  return (float) PIXEL_FORMAT_MONO12_PACKED_MSFIRST;
        case BAYER8:         return (float) PIXEL_FORMAT_BAYER8;
        case BAYER16:        return (float) PIXEL_FORMAT_BAYER16;
        case BAYER10_PACKED: return (float) PIXEL_FORMAT_BAYER10_PACKED_MSFIRST;
        case BAYER12_PACKED: return (float) PIXEL_FORMAT_BAYER12_PACKED_MSFIRST;
        case YUV422:         return (float) PIXEL_FORMAT_YUV422;
        case STOKES:         return (float) PIXEL_FORMAT_STOKES4_12;
        case POLAR:          return (float) PIXEL_FORMAT_POLAR4_12;
        case POLAR_RAW:      return (float) PIXEL_FORMAT_POLAR_RAW4_12;
        case HSV:            return (float) PIXEL_FORMAT_HSV4_12;
        case RGB24:          return (float) PIXEL_FORMAT_RGB24_NON_DIB;
        case BGR24:          return (float) PIXEL_FORMAT_BGR24_NON_DIB;
        case RGBA:           return (float) PIXEL_FORMAT_RGBA;
        case BGRA:           return (float) PIXEL_FORMAT_BGRA;
        case ARGB:           return (float) PIXEL_FORMAT_ARGB;
        case ABGR:           return (float) PIXEL_FORMAT_ABGR;
        default:             return (float) PIXEL_FORMAT_MONO8;
        }
    }

    /**
     * Function: GetBytesPerPixel()
     *
     * Description: Return the number of bits per pixel for a specific Pixel Format
     *
     */
    inline static float bytesPerPixel (U32 pixelFormat)
    {
        switch (pixelFormat)
        {
        case PIXEL_FORMAT_MONO8:
        case PIXEL_FORMAT_BAYER8_RGGB:
        case PIXEL_FORMAT_BAYER8_GRBG:
        case PIXEL_FORMAT_BAYER8_GBRG:
        case PIXEL_FORMAT_BAYER8_BGGR:
            return 1.0f;
        case PIXEL_FORMAT_MONO12_PACKED:
        case PIXEL_FORMAT_BAYER12_RGGB_PACKED:
        case PIXEL_FORMAT_BAYER12_GRBG_PACKED:
        case PIXEL_FORMAT_BAYER12_GBRG_PACKED:
        case PIXEL_FORMAT_BAYER12_BGGR_PACKED:
        case PIXEL_FORMAT_MONO12_PACKED_MSFIRST:
        case PIXEL_FORMAT_BAYER12_RGGB_PACKED_MSFIRST:
        case PIXEL_FORMAT_BAYER12_GRBG_PACKED_MSFIRST:
        case PIXEL_FORMAT_BAYER12_GBRG_PACKED_MSFIRST:
        case PIXEL_FORMAT_BAYER12_BGGR_PACKED_MSFIRST:
            return 1.5f;
        case PIXEL_FORMAT_MONO10_PACKED_MSFIRST:
        case PIXEL_FORMAT_BAYER10_RGGB_PACKED_MSFIRST:
        case PIXEL_FORMAT_BAYER10_GRBG_PACKED_MSFIRST:
        case PIXEL_FORMAT_BAYER10_GBRG_PACKED_MSFIRST:
        case PIXEL_FORMAT_BAYER10_BGGR_PACKED_MSFIRST:
            return 1.25f;
        case PIXEL_FORMAT_MONO16:
        case PIXEL_FORMAT_BAYER16_RGGB:
        case PIXEL_FORMAT_BAYER16_GRBG:
        case PIXEL_FORMAT_BAYER16_GBRG:
        case PIXEL_FORMAT_BAYER16_BGGR:
        case PIXEL_FORMAT_YUV422:
            return 2.0f;
        case PIXEL_FORMAT_RGB24:
        case PIXEL_FORMAT_RGB24_NON_DIB:  // Bugzilla.1239
        case PIXEL_FORMAT_BGR24_NON_DIB:
            return 3.0f;
        case PIXEL_FORMAT_RGBA:
        case PIXEL_FORMAT_BGRA:
        case PIXEL_FORMAT_ARGB:
        case PIXEL_FORMAT_ABGR:
            return 4.0f;

        case PIXEL_FORMAT_RGB48:
        case PIXEL_FORMAT_RGB48_DIB: // Bugzilla.1239
            return 6.0f;
        case PIXEL_FORMAT_STOKES4_12:
        case PIXEL_FORMAT_POLAR4_12:
        case PIXEL_FORMAT_POLAR_RAW4_12:
        case PIXEL_FORMAT_HSV4_12:
            return 6.0f;
        default:
            assert("Invalid pixel format");
        }

        return -1.0f;
    }

};

#endif // !defined(PIXELINK_PIXEL_FORMAT_H)
