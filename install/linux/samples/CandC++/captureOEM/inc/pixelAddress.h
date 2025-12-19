
/***************************************************************************
 *
 *     File: pixelAddress.h
 *
 *     Description: Simple wrapper class for all of the pixel address controls
 *
 *
 */

#if !defined(PIXELINK_PIXEL_ADDRESS_H)
#define PIXELINK_PIXEL_ADDRESS_H

#include "PixeLINKApi.h"

typedef enum _COEM_PIXEL_ADDRESS_MODES
{
   PA_DECIMATE,
   PA_AVERAGE,
   PA_BINNING,
   PA_RESAMPLE
} COEM_PIXEL_ADDRESS_MODES;

typedef enum _COEM_PIXEL_ADDRESS_VALUES
{
    PA_NONE = 0, // Effectively, PA_1_BY_1
    PA_1_BY_2,
    PA_1_BY_3,
    PA_1_BY_4,
    PA_1_BY_6,
    PA_1_BY_8,
    PA_2_BY_1,
    PA_2_BY_2,
    PA_2_BY_3,
    PA_2_BY_4,
    PA_2_BY_6,
    PA_2_BY_8,
    PA_3_BY_1,
    PA_3_BY_2,
    PA_3_BY_3,
    PA_3_BY_4,
    PA_3_BY_6,
    PA_3_BY_8,
    PA_4_BY_1,
    PA_4_BY_2,
    PA_4_BY_3,
    PA_4_BY_4,
    PA_4_BY_6,
    PA_4_BY_8,
    PA_6_BY_1,
    PA_6_BY_2,
    PA_6_BY_3,
    PA_6_BY_4,
    PA_6_BY_6,
    PA_6_BY_8,
    PA_8_BY_1,
    PA_8_BY_2,
    PA_8_BY_3,
    PA_8_BY_4,
    PA_8_BY_6,
    PA_8_BY_8,
} COEM_PIXEL_ADDRESS_VALUES;

class PxLPixelAddress
{
public:
    inline static COEM_PIXEL_ADDRESS_MODES modeFromApi(float apiPixelAddressMode)
    {
        switch ((int)apiPixelAddressMode)
        {
        case PIXEL_ADDRESSING_MODE_DECIMATE: return PA_DECIMATE;
        case PIXEL_ADDRESSING_MODE_AVERAGE:  return PA_AVERAGE;
        case PIXEL_ADDRESSING_MODE_BIN:      return PA_BINNING;
        case PIXEL_ADDRESSING_MODE_RESAMPLE: return PA_RESAMPLE;
        default:                             return PA_DECIMATE;
        }
    }

    inline static float modeToApi (COEM_PIXEL_ADDRESS_MODES pixelAddressMode)
    {
        switch (pixelAddressMode)
        {
        case PA_DECIMATE:  return (float) PIXEL_ADDRESSING_MODE_DECIMATE;
        case PA_AVERAGE:   return (float) PIXEL_ADDRESSING_MODE_AVERAGE;
        case PA_BINNING:   return (float) PIXEL_ADDRESSING_MODE_BIN;
        case PA_RESAMPLE:  return (float) PIXEL_ADDRESSING_MODE_RESAMPLE;
        default:           return (float) PIXEL_ADDRESSING_MODE_DECIMATE; // 'Default' value
        }
    }

    inline static COEM_PIXEL_ADDRESS_VALUES valueFromApi(float apiPixelAddressValueX, float apiPixelAddressValueY)
    {
        // There are 6 possible address values; 1, 2, 3, 4, 6, and 8
        int xOffset = (apiPixelAddressValueX == 8.0f ? 5 : (apiPixelAddressValueX == 6.0f ? 4 : (int)apiPixelAddressValueX-1));
        int yOffset = (apiPixelAddressValueY == 8.0f ? 6 : (apiPixelAddressValueY == 6.0f ? 5 : (int)apiPixelAddressValueY-1));
        return (COEM_PIXEL_ADDRESS_VALUES)(xOffset*6 + yOffset);
    }

    inline static void valueToApi (COEM_PIXEL_ADDRESS_VALUES pixelAddressValue, float* apiPixelAddressValueX, float* apiPixelAddressValueY)
    {
        // There are 6 possible address values; 1, 2, 3, 4, 6, and 8
        int xOffset = pixelAddressValue / 6;
        int yOffset = pixelAddressValue % 6;
        *apiPixelAddressValueX = (xOffset == 5 ? 8.0f : (xOffset == 4 ? 6.0f : (float)xOffset+1));
        *apiPixelAddressValueY = (yOffset == 5 ? 8.0f : (yOffset == 4 ? 6.0f : (float)yOffset+1));
    }
};


#endif // !defined(PIXELINK_PIXEL_ADDRESS_H)
