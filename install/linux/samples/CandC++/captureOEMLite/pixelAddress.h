
/***************************************************************************
 *
 *     File: pixelAddress.h
 *
 *     Description: Simple wrapper class for all of the pixel address controls
 *
 *     Design Notes:
 *          - As a simplification, this application assumes symmetric pixel addressing.
 *            That is, it will always perform he same pixel addressing to the X and the
 *            Y dimension (width and height).  This is done just as a simplification to
 *            the application.  Some PixeLINK cameras support asymmetric pixel addressing,
 *            where you can independently control the pixel addressing value to the X and Y
 *            directions.
 *
 */

#if !defined(PIXELINK_PIXEL_ADDRESS_H)
#define PIXELINK_PIXEL_ADDRESS_H

#include "PixeLINKApi.h"
#include <gtk/gtk.h>
#include <vector>

typedef enum _PXL_PIXEL_ADDRESS_MODES
{
   PA_DECIMATE,
   PA_AVERAGE,
   PA_BINNING,
   PA_RESAMPLE
} PXL_PIXEL_ADDRESS_MODES;

typedef enum _PXL_PIXEL_ADDRESS_VALUES
{
    PA_NONE = 0,
    PA_2_BY_2,
    PA_3_BY_3,
    PA_4_BY_4,
    PA_6_BY_6,
    PA_8_BY_8
} PXL_PIXEL_ADDRESS_VALUES;

static const char * const PxLAddressModes[] =
{
   "Decimate",
   "Average",
   "Binning",
   "Resample"
};

static const char * const PxLAddressValues[] =
{
   "None",
   "2 by 2",
   "3 by 3",
   "4 by 4",
   "6 by 6",
   "8 by 8"
};

class PxLPixelAddress
{
public:
    // Constructor
    PxLPixelAddress (GtkWidget *modeCombo, GtkWidget *valueCombo);
	// Destructor
	~PxLPixelAddress ();

    void  greyAll ();
    void  initialize ();

    GtkWidget    *m_modeCombo;
    GtkWidget    *m_valueCombo;
    std::vector<PXL_PIXEL_ADDRESS_MODES>  m_modeComboEntries;
    std::vector<PXL_PIXEL_ADDRESS_VALUES> m_valueComboEntries;

    // simple routines to convert pixel address mode to/from the ones used by the PixeLINK API
    PXL_PIXEL_ADDRESS_MODES modeFromApi (float apiPixelAddressMode);
    float                   modeToApi (PXL_PIXEL_ADDRESS_MODES pixeladdressMode);
    // simple routines to convert pixel address values to/from the ones used by the PixeLINK API
    PXL_PIXEL_ADDRESS_VALUES valueFromApi (float apiPixelAddressValue);
    float                    valueToApi (PXL_PIXEL_ADDRESS_VALUES pixeladdressValue);

};

inline PXL_PIXEL_ADDRESS_MODES PxLPixelAddress::modeFromApi(float apiPixelAddressMode)
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

inline float PxLPixelAddress::modeToApi (PXL_PIXEL_ADDRESS_MODES pixelAddressMode)
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

inline PXL_PIXEL_ADDRESS_VALUES PxLPixelAddress::valueFromApi(float apiPixelAddressValue)
{
    switch ((int)apiPixelAddressValue)
    {
    case 0: return PA_NONE;
    case 1: return PA_NONE;
    case 2: return PA_2_BY_2;
    case 3: return PA_3_BY_3;
    case 4: return PA_4_BY_4;
    case 6: return PA_6_BY_6;
    case 8: return PA_8_BY_8;
    default: return PA_NONE;
    }
}

inline float PxLPixelAddress::valueToApi (PXL_PIXEL_ADDRESS_VALUES pixelAddressValue)
{
    switch (pixelAddressValue)
    {
    case PA_NONE:    return 1.0f;
    case PA_2_BY_2:  return 2.0f;
    case PA_3_BY_3:  return 3.0f;
    case PA_4_BY_4:  return 4.0f;
    case PA_6_BY_6:  return 6.0f;
    case PA_8_BY_8:  return 8.0f;
    default:         return 1.0f; // 'Default' value
    }
}

#endif // !defined(PIXELINK_PIXEL_ADDRESS_H)
