
/***************************************************************************
 *
 *     File: roi.h
 *
 *     Description:
 *         Helper module for wrking with ROIs
 *
 */

#if !defined(PIXELINK_ROI_H)
#define PIXELINK_ROI_H

#include "PixeLINKApi.h"

typedef enum _ROI_TYPE
{
   FrameRoi,
   SharpnessScoreRoi,
   AutoRoi
} ROI_TYPE;

typedef struct _ROI_LIMITS
{
    int xMin;
    int xMax;
    int yMin;
    int yMax;
} ROI_LIMITS, *PROI_LIMITS;

// Operations that can be performed on the ROI Button
typedef enum _ROI_BUTTON_OPS
{
    OP_NONE,
    OP_MOVE,
    OP_RESIZE_TOP_LEFT,
    OP_RESIZE_TOP_RIGHT,
    OP_RESIZE_BOTTOM_RIGHT,
    OP_RESIZE_BOTTOM_LEFT,
    OP_RESIZE_TOP,
    OP_RESIZE_RIGHT,
    OP_RESIZE_BOTTOM,
    OP_RESIZE_LEFT
} ROI_BUTTON_OPS;


class PXL_ROI
{
public:
    // Note that the operators only look at size, not the offset
    bool operator==(const PXL_ROI& rhs) {return (rhs.m_width == this->m_width && rhs.m_height==this->m_height);}
    bool operator!=(const PXL_ROI& rhs) {return !operator == (rhs);}
    bool operator<(const PXL_ROI& rhs) {return ((this->m_width < rhs.m_width) || (this->m_height < rhs.m_height));}
    bool operator>(const PXL_ROI& rhs) {return ((this->m_width > rhs.m_width) || (this->m_height > rhs.m_height));}
    int m_width;
    int m_height;
    int m_offsetX;
    int m_offsetY;
    PXL_ROI (int width = 0, int height = 0, int offsetX = 0, int offsetY = 0):
        m_width(width), m_height(height), m_offsetX(offsetX), m_offsetY(offsetY)
        {};
    void rotateClockwise (int degrees);
    void rotateClockwise (int degrees, PXL_ROI& max);
    void rotateCounterClockwise (int degrees);
    void rotateCounterClockwise (int degrees, PXL_ROI& max);
    void flipVertical (PXL_ROI& max);
    void flipHorizontal (PXL_ROI& max);
};

inline void PXL_ROI::rotateClockwise(int degrees)
{
    if ((degrees == 90) || (degrees == 270)) std::swap (m_width, m_height);
}

inline void PXL_ROI::rotateClockwise(int degrees, PXL_ROI& max)
{
    PXL_ROI orig = *this;

    switch (degrees) {
    case 90:
        m_offsetX = max.m_height - orig.m_height - orig.m_offsetY;
        m_offsetY = orig.m_offsetX;
        break;
    case 180:
        m_offsetX = max.m_width - orig.m_width - orig.m_offsetX;
        m_offsetY = max.m_height - orig.m_height - orig.m_offsetY;
        break;
    case 270:
        m_offsetX = orig.m_offsetY;
        m_offsetY = max.m_width - orig.m_width - orig.m_offsetX;
        break;
    default:
        break;
    }
    this->rotateClockwise(degrees);
}

inline void PXL_ROI::rotateCounterClockwise(int degrees)
{
    if ((degrees == 90) || (degrees == 270)) std::swap (m_width, m_height);
}

inline void PXL_ROI::rotateCounterClockwise(int degrees, PXL_ROI& max)
{
    PXL_ROI orig = *this;

    switch (degrees) {
    case 90:
        m_offsetX = orig.m_offsetY;
        m_offsetY = max.m_height - orig.m_width - orig.m_offsetX;
        break;
    case 180:
        m_offsetX = max.m_width - orig.m_width - orig.m_offsetX;
        m_offsetY = max.m_height - orig.m_height - orig.m_offsetY;
        break;
    case 270:
        m_offsetX = max.m_width - orig.m_height - orig.m_offsetY;
        m_offsetY = orig.m_offsetX;
        break;
    default:
        break;
    }
    this->rotateCounterClockwise(degrees);
}

inline void PXL_ROI::flipVertical (PXL_ROI& max)
{
    m_offsetY = max.m_height - m_height - m_offsetY;
}

inline void PXL_ROI::flipHorizontal (PXL_ROI& max)
{
    m_offsetX = max.m_width - m_width - m_offsetX;
}

extern std::vector<PXL_ROI> PxLStandardRois;


#endif // !defined(PIXELINK_ROI_H)
