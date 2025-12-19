/*
 * callbacks.cpp
 *
 *  Created on: Mar 5, 2018
 *      Author: pcarroll
 *
 *  Description
 *      All of the callbacks used by filter.cpp  These were taken from the Windows
 *      variant of Capture OEM
 */

#include <string.h>
#include <vector>
#include <glib.h>
#include <algorithm>
#include <SDL2/SDL.h>
#include "filter.h"
#include "pixelFormat.h"

using namespace std;


// Macro to calculate decimated width or height of the ROI:
#define DEC_SIZE(len,dec) (((len) + (dec) - 1) / (dec))

#define DCAM16_TO_TENBIT(x) ((((x) & 0x00FF) << 2) | ((x) >> 14))
#define TENBIT_TO_DCAM16(x) ((((x) & 0x03FC) >> 2) | ((x) << 14))

struct RGBPixel
{
    U8 R,G,B;
    RGBPixel() : R(0),G(0),B(0) {}
    RGBPixel(U8 r, U8 g, U8 b) : R(r),G(g),B(b) {}
};
RGBPixel operator+(RGBPixel const& rgb, int val) { return RGBPixel(rgb.R+val, rgb.G+val, rgb.B+val); }
RGBPixel operator+(int val, RGBPixel const& rgb) { return RGBPixel(rgb.R+val, rgb.G+val, rgb.B+val); }
RGBPixel operator+(RGBPixel const& rgb1, RGBPixel const& rgb2) { return RGBPixel(rgb1.R+rgb2.R, rgb1.G+rgb2.G, rgb1.B+rgb2.B); }
RGBPixel operator*(RGBPixel const& rgb, int val) { return RGBPixel(rgb.R*val, rgb.G*val, rgb.B*val); }
RGBPixel operator*(int val, RGBPixel const& rgb) { return RGBPixel(rgb.R*val, rgb.G*val, rgb.B*val); }
RGBPixel operator/(RGBPixel const& rgb, int val) { return RGBPixel(rgb.R/val, rgb.G/val, rgb.B/val); }

RGBPixel
abs(RGBPixel const& rgb)
{
    return RGBPixel(abs(rgb.R), abs(rgb.G), abs(rgb.B));
}

// Round off a floating point number.
template<typename T, typename U>
T round_to(U val)
{
    return static_cast<T>(val + 0.5);
}

// RGB <==> YUV conversions
template<typename T1, typename T2>
inline void RGBtoYUV(T1 const* RGB, T2* YUV)
{
    int Y = static_cast<int>( ( 0.299000 * RGB[0]) + ( 0.615000 * RGB[1]) + ( 0.114000 * RGB[2]) + 0  );
    int U = static_cast<int>( (-0.168077 * RGB[0]) + (-0.329970 * RGB[1]) + ( 0.498047 * RGB[2]) + 128);
    int V = static_cast<int>( ( 0.498047 * RGB[0]) + (-0.417052 * RGB[1]) + (-0.080994 * RGB[2]) + 128);
    YUV[0] = round_to<T2>(min(max(Y,0),255));
    YUV[1] = round_to<T2>(min(max(U,0),255));
    YUV[2] = round_to<T2>(min(max(V,0),255));
}

template<typename T1, typename T2>
inline void BGRtoYUV(T1 const* BGR, T2* YUV)
{
    T1 RGB[3] = {BGR[2], BGR[1], BGR[0]};
    RGBtoYUV(&RGB[0], YUV);
}

//  | 1     -1.223674E-6    1.407498    |
//  | 1     -0.345485       -0.716937   |
//  | 1     1.778949        4.078914E-7 |
template<typename T1, typename T2>
inline void YUVtoRGB(T1 const* YUV, T2* RGB)
{
    int R = static_cast<int>( YUV[0] + (-1.223674e-6 * (YUV[1]-128)) + ( 1.407498    * (YUV[2]-128)) );
    int G = static_cast<int>( YUV[0] + (-0.345485    * (YUV[1]-128)) + (-0.716937    * (YUV[2]-128)) );
    int B = static_cast<int>( YUV[0] + (1.778949     * (YUV[1]-128)) + ( 4.078914e-7 * (YUV[2]-128)) );
    RGB[0] = static_cast<T2>(min(max(R,0),255));
    RGB[1] = static_cast<T2>(min(max(G,0),255));
    RGB[2] = static_cast<T2>(min(max(B,0),255));
}

template<typename T1, typename T2>
inline void YUVtoBGR(T1 const* YUV, T2* BGR)
{
    T2 RGB[3] = {0,0,0};
    YUVtoRGB(YUV, &RGB[0]);
    BGR[0] = RGB[2];
    BGR[1] = RGB[1];
    BGR[2] = RGB[0];
}

template<typename T>
void Convolution_3x3(int const* kernel, T* pData, const int width, const int height)
{
    int normalizer = 0;
    for (int i = 0; i < 9; i++)
    {
        normalizer += kernel[i];
    }
    normalizer = abs(normalizer);
    if (normalizer == 0)
    {
        normalizer = 1;
    }


    // Make a buffer to hold the unaltered values of the previous row.
    std::vector<T> rowBuffer(width);
    T prevVal;

    // Copy row 0 into the rowBuffer.
    memcpy(&rowBuffer[0], pData, width * sizeof(T));

    for (int y = 1; y < height-1; y++)
    {
        // Copy the "about-to-be-altered" row into the rowBuffer
        memcpy(&rowBuffer[0], pData + (y*width), width * sizeof(T));
        T* prevRow = &rowBuffer[0];
        T* thisRow = pData + (y * width);
        T* nextRow = pData + ((y+1) * width);
        prevVal = thisRow[0];
        for (int x = 1; x < width-1; x++)
        {
            /*
            T newVal = 0;
            for (int i = 0; i < 3; i++)
            {
                // Get the first two rows from the rowBuffer;
                newVal += kernel[i] * prevRow[x+i-1];
                newVal += kernel[3+i] * thisRow[x+i-1];
                // Get the last row from the actual data - it hasn't been overwritten yet.
                newVal += kernel[6+i] * pData[(y+1)*width + (x+i-1)];
            }
            */
            T temp = thisRow[x];
            thisRow[x] = abs(
                    kernel[0] * prevRow[x-1] +
                    kernel[1] * prevRow[x] +
                    kernel[2] * prevRow[x+1] +
                    kernel[3] * prevVal +
                    kernel[4] * thisRow[x] +
                    kernel[5] * thisRow[x+1] +
                    kernel[6] * nextRow[x-1] +
                    kernel[7] * nextRow[x] +
                    kernel[8] * nextRow[x+1]
                ) / normalizer;
            prevVal = temp;
        }
    }
}

void
Convolution_3x3_12Bit_Packed(int const* kernel, U8* pData, const int width, const int height, bool msFirst)
// see Design Notes above on special handling of 12 bit packed formats.
{
    int normalizer = 0;
    int widthPlusHalf = width + width/2;
    for (int i = 0; i < 9; i++)
    {
        normalizer += kernel[i];
    }
    normalizer = abs(normalizer);
    if (normalizer == 0)
    {
        normalizer = 1;
    }


    // Make a buffer to hold the unaltered values of the previous row.
    std::vector<U8> rowBuffer(widthPlusHalf);
    U8              prevVal;
    int             skipByte = msFirst ? 2 : 1;  //Are we to skip every 3rd (msFirst) or 2nd (normal) byte?

    // Copy row 0 into the rowBuffer.
    memcpy(&rowBuffer[0], pData, widthPlusHalf);

    for (int y = 1; y < height-1; y++)
    {
        // Copy the "about-to-be-altered" row into the rowBuffer
        memcpy(&rowBuffer[0], pData + (y*widthPlusHalf), widthPlusHalf);
        U8* prevRow = &rowBuffer[0];
        U8* thisRow = pData + (y * widthPlusHalf);
        U8* nextRow = pData + ((y+1) * widthPlusHalf);
        prevVal = thisRow[0];
        for (int x = 1; x < widthPlusHalf-1; x++)
        {
            if (x % 3 == skipByte)
            {
                // Ignore the LS bits
                thisRow[x] = 0;
                continue;
            }
            U8 temp = thisRow[x];
            if ((x & 1) != msFirst)
            {
                // Ignore the previous 'column' -- only has LS bits
                thisRow[x] = abs(
                        kernel[1] * prevRow[x] +
                        kernel[2] * prevRow[x+1] +
                        kernel[4] * thisRow[x] +
                        kernel[5] * thisRow[x+1] +
                        kernel[7] * nextRow[x] +
                        kernel[8] * nextRow[x+1]
                ) / normalizer;
            } else {
                // Ignore the next 'column' -- only has LS bits
                thisRow[x] = abs(
                        kernel[0] * prevRow[x-1] +
                        kernel[1] * prevRow[x] +
                        kernel[3] * prevVal +
                        kernel[4] * thisRow[x] +
                        kernel[6] * nextRow[x-1] +
                        kernel[7] * nextRow[x]
                    ) / normalizer;
            }
            prevVal = temp;
        }
    }
}

void
Convolution_3x3_10Bit_Packed(int const* kernel, U8* pData, const int width, const int height)
// see Design Notes above on special handling of 10 bit packed formats.
{
    int normalizer = 0;
    int widthPlusQuarter = width + width/4;
    for (int i = 0; i < 9; i++)
    {
        normalizer += kernel[i];
    }
    normalizer = abs(normalizer);
    if (normalizer == 0)
    {
        normalizer = 1;
    }


    // Make a buffer to hold the unaltered values of the previous row.
    std::vector<U8> rowBuffer(widthPlusQuarter);
    U8              prevVal;
    int             div5;  //We to skip every 5th byte

    // Copy row 0 into the rowBuffer.
    memcpy(&rowBuffer[0], pData, widthPlusQuarter);

    for (int y = 1; y < height-1; y++)
    {
        // Copy the "about-to-be-altered" row into the rowBuffer
        memcpy(&rowBuffer[0], pData + (y*widthPlusQuarter), widthPlusQuarter);
        U8* prevRow = &rowBuffer[0];
        U8* thisRow = pData + (y * widthPlusQuarter);
        U8* nextRow = pData + ((y+1) * widthPlusQuarter);
        prevVal = thisRow[0];
        for (int x = 1; x < widthPlusQuarter-1; x++)
        {
            div5 = x % 5;
            U8 temp = thisRow[x];
            switch (div5) {
            case 0:
                // Ignore the previous 'column' -- only has LS bits
                thisRow[x] = abs(
                        kernel[1] * prevRow[x] +
                        kernel[2] * prevRow[x+1] +
                        kernel[4] * thisRow[x] +
                        kernel[5] * thisRow[x+1] +
                        kernel[7] * nextRow[x] +
                        kernel[8] * nextRow[x+1]
                ) / normalizer;
                break;
            case 3:
                // Ignore the next 'column' -- only has LS bits
                thisRow[x] = abs(
                        kernel[0] * prevRow[x-1] +
                        kernel[1] * prevRow[x] +
                        kernel[3] * prevVal +
                        kernel[4] * thisRow[x] +
                        kernel[6] * nextRow[x-1] +
                        kernel[7] * nextRow[x]
                    ) / normalizer;
                break;
            case 4:
                // Ignore this 'column' -- it only has LS bits
                thisRow[x] = 0;
                break;
            default:
                thisRow[x] = abs(
                        kernel[0] * prevRow[x-1] +
                        kernel[1] * prevRow[x] +
                        kernel[2] * prevRow[x+1] +
                        kernel[3] * prevVal +
                        kernel[4] * thisRow[x] +
                        kernel[5] * thisRow[x+1] +
                        kernel[6] * nextRow[x-1] +
                        kernel[7] * nextRow[x] +
                        kernel[8] * nextRow[x+1]
                    ) / normalizer;
                break;
            }
            prevVal = temp;
        }
    }
}

void
Convolution_3x3_RGB(int const * const kernel, U8* pData, const int width, const int height)
{
    int normalizer = 0;
    for (int i = 0; i < 9; i++)
    {
        normalizer += kernel[i];
    }
    normalizer = abs(normalizer);
    if (normalizer == 0)
    {
        normalizer = 1;
    }

    int bytewidth = width * 3;

    // Make a buffer to hold the unaltered values of the previous row and the
    // current row.
    U8* buffer = new U8[bytewidth * 2];

    // Copy row 0 into the buffer.
    memcpy(buffer, pData, bytewidth * sizeof(U8));

    for (int y = 1; y < height-1; y++)
    {
        // Copy the "about-to-be-altered" row into the buffer
        memcpy(&buffer[bytewidth*(y%2)], &pData[y*bytewidth], bytewidth);
        for (int x = 1; x < width-1; x++)
        {
            U8* prevRow = &buffer[bytewidth * ((y+1)%2)];
            U8* thisRow = &buffer[bytewidth * (y%2)];
            for (int rgb = 0; rgb < 3; rgb++)
            {
                int newVal = 0;
                for (int i = 0; i < 3; i++)
                {
                    // Get the first two rows from the buffer;
                    newVal += kernel[i] * prevRow[3*(x+i-1) + rgb];
                    newVal += kernel[3+i] * thisRow[3*(x+i-1) + rgb];
                    // Get the last row from the actual data - it hasn't been overwritten yet.
                    newVal += kernel[6+i] * pData[(y+1)*bytewidth + 3*(x+i-1) + rgb];
                }
                pData[y*bytewidth + 3*x + rgb] = static_cast<U8>(abs(newVal)/normalizer);
            }
        }
    }
}

const int highpass_kernel_3x3[] =
{
    -1,     -1,     -1,
    -1,     9,      -1,
    -1,     -1,     -1
};

const int lowpass_kernel_3x3[] =
{
    1,      2,      1,
    2,      4,      2,
    1,      2,      1
};

const int sobel_vertical_3x3[] =
{
    -1,     0,      1,
    -2,     0,      2,
    -1,     0,      1
};

const int sobel_horizontal_3x3[] =
{
    -1,     -2,     -1,
    0,      0,      0,
    1,      2,      1
};

#define PXLAPI_CALLBACK(funcname)                       \
    U32 funcname( HANDLE hCamera,             \
                  LPVOID pFrameData,          \
                  U32 uDataFormat,            \
                  FRAME_DESC const * pFrameDesc,      \
                  LPVOID pContext)            \

template<typename T>void MedianFilter_3x3_Impl(T* const pData, T const * const pCopy, const int width, const int height)
{
    int buf[9];
    for (int y = 1; y < height-1; y++)
    {
        for (int x = 1; x < width-1; x++)
        {
            for (int yy = -1; yy <= 1; yy++)
            {
                for (int xx = -1; xx <= 1; xx++)
                {
                    // This could be made faster - too much going on in the inner loop here...
                    buf[3*(yy+1) + (xx+1)] = pCopy[(y+yy)*width + (x+xx)];
                }
            }
            std::sort(&buf[0], &buf[9]);
            pData[y*width + x] = static_cast<T>(buf[4]);
        }
    }
}

// See Design Notes above on special handling of 12-bit packed format
void
MedianFilter_3x3_12Bit_Packed_Impl(U8* const pData, U8 const * const pCopy, const int width, const int height, bool msFirst)
{
    int  buf[6];
    int* pBuf;
    int widthPlusHalf = width + width/2;
    int skipByte = msFirst ? 2 : 1;  //Are we to skip every 3rd (msFirst) or 2nd (normal) byte?

    for (int y = 1; y < height-1; y++)
    {
        for (int x = 1; x < widthPlusHalf-1; x++)
        {
            if (x % 3 == skipByte)
            {
                // Ignore the LS bits
                pData[y*widthPlusHalf + x] = 0;
                continue;
            }

            pBuf = &buf[0];
            for (int yy = -1; yy <= 1; yy++)
            {
                for (int xx = -1; xx <= 1; xx++)
                {
                    if ( (x & !msFirst) && xx ==  1) continue; // Skip bytes containing LS bits (col 1)
                    if (!(x & !msFirst) && xx == -1) continue; // Skip bytes containing LS bits (col -1)
                    *pBuf++ = pCopy[(y+yy)*widthPlusHalf + (x+xx)];
                }
            }
            std::sort(&buf[0], &buf[5]);
            pData[y*widthPlusHalf + x] = static_cast<U8>((buf[2] + buf[3])/2);  // average the middle two
        }
    }
}

// See Design Notes above on special handling of 10-bit packed format
void
MedianFilter_3x3_10Bit_Packed_Impl(U8* const pData, U8 const * const pCopy, const int width, const int height)
{
    int  buf[6];
    int* pBuf;
    int widthPlusQuarter = width + width/4;
    int div5;  //we to skip every 5th byte

    for (int y = 1; y < height-1; y++)
    {
        for (int x = 1; x < widthPlusQuarter-1; x++)
        {
            div5 = x % 5;
            if (div5 == 4)
            {
                // Ignore the LS bits
                pData[y*widthPlusQuarter + x] = 0;
                continue;
            }

            pBuf = &buf[0];
            for (int yy = -1; yy <= 1; yy++)
            {
                for (int xx = -1; xx <= 1; xx++)
                {
                    if (div5 == 3 && xx ==  1) continue; // Skip bytes containing LS bits (col 1)
                    if (div5 == 0 && xx == -1) continue; // Skip bytes containing LS bits (col -1)
                    *pBuf++ = pCopy[(y+yy)*widthPlusQuarter + (x+xx)];
                }
            }
            std::sort(&buf[0], &buf[5]);
            pData[y*widthPlusQuarter + x] = static_cast<U8>((buf[2] + buf[3])/2);  // average the middle two
        }
    }
}

void
MedianFilter_3x3_RGB_Impl(U8* const pData, U8 const * const pCopy, const int width, const int height)
{
    int buf[9];
    for (int y = 1; y < height-1; y++)
    {
        for (int x = 1; x < width-1; x++)
        {
            for (int rgb = 0; rgb < 3; rgb++)
            {
                for (int yy = -1; yy <= 1; yy++)
                {
                    for (int xx = -1; xx <= 1; xx++)
                    {
                        // This could be made faster - too much going on in the inner loop here...
                        buf[3*(yy+1) + (xx+1)] = pCopy[3*(y+yy)*width + 3*(x+xx) + rgb];
                    }
                }
                std::sort(&buf[0], &buf[9]);
                pData[3*(y*width + x) + rgb] = static_cast<U8>(buf[4]);
            }
        }
    }
}

PXLAPI_CALLBACK(PxLCallbackMedian)
{
    int decX = max(1, static_cast<int>(pFrameDesc->PixelAddressingValue.fHorizontal));
    int decY = max(1, static_cast<int>(pFrameDesc->PixelAddressingValue.fVertical));
    int decWidth = DEC_SIZE(static_cast<int>(pFrameDesc->Roi.fWidth), decX);
    int decHeight = DEC_SIZE(static_cast<int>(pFrameDesc->Roi.fHeight), decY);
    float pixelSize = PxLPixelFormat::bytesPerPixel(static_cast<int>(uDataFormat));
    int bufferSize = static_cast<int> (static_cast<float>(decWidth) * static_cast<float>(decHeight) * pixelSize);

    // Allocate enough memory to hold a copy of the frame.
    std::vector<U8> buffer(bufferSize);
    memcpy(&buffer[0], pFrameData, bufferSize);

    switch (uDataFormat)
    {
    case PIXEL_FORMAT_MONO8:
    case PIXEL_FORMAT_BAYER8_BGGR:
    case PIXEL_FORMAT_BAYER8_GBRG:
    case PIXEL_FORMAT_BAYER8_GRBG:
    case PIXEL_FORMAT_BAYER8_RGGB:
        MedianFilter_3x3_Impl<U8>(static_cast<U8*>(pFrameData),
                                  static_cast<U8*>(&buffer[0]),
                                  decWidth,
                                  decHeight);
        break;

    case PIXEL_FORMAT_MONO16:
    case PIXEL_FORMAT_BAYER16_BGGR:
    case PIXEL_FORMAT_BAYER16_GBRG:
    case PIXEL_FORMAT_BAYER16_GRBG:
    case PIXEL_FORMAT_BAYER16_RGGB:
        MedianFilter_3x3_Impl<U16>(static_cast<U16*>(pFrameData),
                                   reinterpret_cast<U16*>(&buffer[0]),
                                   decWidth,
                                   decHeight);
        break;

    case PIXEL_FORMAT_MONO12_PACKED:
    case PIXEL_FORMAT_BAYER12_BGGR_PACKED:
    case PIXEL_FORMAT_BAYER12_GBRG_PACKED:
    case PIXEL_FORMAT_BAYER12_GRBG_PACKED:
    case PIXEL_FORMAT_BAYER12_RGGB_PACKED:
        MedianFilter_3x3_12Bit_Packed_Impl(static_cast<U8*>(pFrameData),
                                   reinterpret_cast<U8*>(&buffer[0]),
                                   decWidth,
                                   decHeight,
                                   FALSE);
        break;

    case PIXEL_FORMAT_MONO12_PACKED_MSFIRST:
    case PIXEL_FORMAT_BAYER12_BGGR_PACKED_MSFIRST:
    case PIXEL_FORMAT_BAYER12_GBRG_PACKED_MSFIRST:
    case PIXEL_FORMAT_BAYER12_GRBG_PACKED_MSFIRST:
    case PIXEL_FORMAT_BAYER12_RGGB_PACKED_MSFIRST:
        MedianFilter_3x3_12Bit_Packed_Impl(static_cast<U8*>(pFrameData),
                                   reinterpret_cast<U8*>(&buffer[0]),
                                   decWidth,
                                   decHeight,
                                   TRUE);
        break;

    case PIXEL_FORMAT_MONO10_PACKED_MSFIRST:
    case PIXEL_FORMAT_BAYER10_BGGR_PACKED_MSFIRST:
    case PIXEL_FORMAT_BAYER10_GBRG_PACKED_MSFIRST:
    case PIXEL_FORMAT_BAYER10_GRBG_PACKED_MSFIRST:
    case PIXEL_FORMAT_BAYER10_RGGB_PACKED_MSFIRST:
        MedianFilter_3x3_10Bit_Packed_Impl(static_cast<U8*>(pFrameData),
                                   reinterpret_cast<U8*>(&buffer[0]),
                                   decWidth,
                                   decHeight);
        break;

    case PIXEL_FORMAT_RGB24:
    case PIXEL_FORMAT_RGB24_NON_DIB:
    case PIXEL_FORMAT_BGR24_NON_DIB:
        MedianFilter_3x3_RGB_Impl(static_cast<U8*>(pFrameData),
                                  static_cast<U8*>(&buffer[0]),
                                  decWidth,
                                  decHeight);
        break;

    default:
        return ApiInvalidParameterError;
    }

    return ApiSuccess;
}

PXLAPI_CALLBACK(PxLCallbackLowPass)
{
    int decX = static_cast<int>(pFrameDesc->PixelAddressingValue.fHorizontal);
    int decY = static_cast<int>(pFrameDesc->PixelAddressingValue.fVertical);
    int width = DEC_SIZE(static_cast<int>(pFrameDesc->Roi.fWidth), decX);
    int height = DEC_SIZE(static_cast<int>(pFrameDesc->Roi.fHeight), decY);

    switch (uDataFormat)
    {
    case PIXEL_FORMAT_MONO8:
    case PIXEL_FORMAT_BAYER8_BGGR:
    case PIXEL_FORMAT_BAYER8_GBRG:
    case PIXEL_FORMAT_BAYER8_GRBG:
    case PIXEL_FORMAT_BAYER8_RGGB:
        Convolution_3x3<U8>(&lowpass_kernel_3x3[0],
                            static_cast<U8*>(pFrameData),
                            width,
                            height);
        break;

    case PIXEL_FORMAT_MONO16:
    case PIXEL_FORMAT_BAYER16_BGGR:
    case PIXEL_FORMAT_BAYER16_GBRG:
    case PIXEL_FORMAT_BAYER16_GRBG:
    case PIXEL_FORMAT_BAYER16_RGGB:
        Convolution_3x3<U16>(&lowpass_kernel_3x3[0],
                             static_cast<U16*>(pFrameData),
                             width,
                             height);
        break;

    case PIXEL_FORMAT_MONO12_PACKED:
    case PIXEL_FORMAT_BAYER12_BGGR_PACKED:
    case PIXEL_FORMAT_BAYER12_GBRG_PACKED:
    case PIXEL_FORMAT_BAYER12_GRBG_PACKED:
    case PIXEL_FORMAT_BAYER12_RGGB_PACKED:
        Convolution_3x3_12Bit_Packed(&lowpass_kernel_3x3[0],
                             static_cast<U8*>(pFrameData),
                             width,
                             height,
                             FALSE);
        break;

    case PIXEL_FORMAT_MONO12_PACKED_MSFIRST:
    case PIXEL_FORMAT_BAYER12_BGGR_PACKED_MSFIRST:
    case PIXEL_FORMAT_BAYER12_GBRG_PACKED_MSFIRST:
    case PIXEL_FORMAT_BAYER12_GRBG_PACKED_MSFIRST:
    case PIXEL_FORMAT_BAYER12_RGGB_PACKED_MSFIRST:
        Convolution_3x3_12Bit_Packed(&lowpass_kernel_3x3[0],
                             static_cast<U8*>(pFrameData),
                             width,
                             height,
                             TRUE);
        break;

    case PIXEL_FORMAT_MONO10_PACKED_MSFIRST:
    case PIXEL_FORMAT_BAYER10_BGGR_PACKED_MSFIRST:
    case PIXEL_FORMAT_BAYER10_GBRG_PACKED_MSFIRST:
    case PIXEL_FORMAT_BAYER10_GRBG_PACKED_MSFIRST:
    case PIXEL_FORMAT_BAYER10_RGGB_PACKED_MSFIRST:
        Convolution_3x3_10Bit_Packed(&lowpass_kernel_3x3[0],
                             static_cast<U8*>(pFrameData),
                             width,
                             height);
        break;

    case PIXEL_FORMAT_RGB24:
    case PIXEL_FORMAT_RGB24_NON_DIB:
    case PIXEL_FORMAT_BGR24_NON_DIB:
        Convolution_3x3_RGB(&lowpass_kernel_3x3[0],
                            static_cast<U8*>(pFrameData),
                            width,
                            height);
        break;
    default:
        return ApiInvalidParameterError;
    }

    return ApiSuccess;
}

PXLAPI_CALLBACK(PxLCallbackHighPass)
{
    int decX = static_cast<int>(pFrameDesc->PixelAddressingValue.fHorizontal);
    int decY = static_cast<int>(pFrameDesc->PixelAddressingValue.fVertical);
    int width = DEC_SIZE(static_cast<int>(pFrameDesc->Roi.fWidth), decX);
    int height = DEC_SIZE(static_cast<int>(pFrameDesc->Roi.fHeight), decY);

    switch (uDataFormat)
    {
    case PIXEL_FORMAT_MONO8:
    case PIXEL_FORMAT_BAYER8_BGGR:
    case PIXEL_FORMAT_BAYER8_GBRG:
    case PIXEL_FORMAT_BAYER8_GRBG:
    case PIXEL_FORMAT_BAYER8_RGGB:
        Convolution_3x3<U8>(&highpass_kernel_3x3[0],
                            static_cast<U8*>(pFrameData),
                            width,
                            height);
        break;

    case PIXEL_FORMAT_MONO16:
    case PIXEL_FORMAT_BAYER16_BGGR:
    case PIXEL_FORMAT_BAYER16_GBRG:
    case PIXEL_FORMAT_BAYER16_GRBG:
    case PIXEL_FORMAT_BAYER16_RGGB:
        Convolution_3x3<U16>(&highpass_kernel_3x3[0],
                             static_cast<U16*>(pFrameData),
                             width,
                             height);
        break;

    case PIXEL_FORMAT_MONO12_PACKED:
    case PIXEL_FORMAT_BAYER12_BGGR_PACKED:
    case PIXEL_FORMAT_BAYER12_GBRG_PACKED:
    case PIXEL_FORMAT_BAYER12_GRBG_PACKED:
    case PIXEL_FORMAT_BAYER12_RGGB_PACKED:
        Convolution_3x3_12Bit_Packed(&highpass_kernel_3x3[0],
                             static_cast<U8*>(pFrameData),
                             width,
                             height,
                             FALSE);
        break;

    case PIXEL_FORMAT_MONO12_PACKED_MSFIRST:
    case PIXEL_FORMAT_BAYER12_BGGR_PACKED_MSFIRST:
    case PIXEL_FORMAT_BAYER12_GBRG_PACKED_MSFIRST:
    case PIXEL_FORMAT_BAYER12_GRBG_PACKED_MSFIRST:
    case PIXEL_FORMAT_BAYER12_RGGB_PACKED_MSFIRST:
        Convolution_3x3_12Bit_Packed(&highpass_kernel_3x3[0],
                             static_cast<U8*>(pFrameData),
                             width,
                             height,
                             TRUE);
        break;

    case PIXEL_FORMAT_MONO10_PACKED_MSFIRST:
    case PIXEL_FORMAT_BAYER10_BGGR_PACKED_MSFIRST:
    case PIXEL_FORMAT_BAYER10_GBRG_PACKED_MSFIRST:
    case PIXEL_FORMAT_BAYER10_GRBG_PACKED_MSFIRST:
    case PIXEL_FORMAT_BAYER10_RGGB_PACKED_MSFIRST:
        Convolution_3x3_10Bit_Packed(&highpass_kernel_3x3[0],
                             static_cast<U8*>(pFrameData),
                             width,
                             height);
        break;

    case PIXEL_FORMAT_RGB24:
    case PIXEL_FORMAT_RGB24_NON_DIB:
    case PIXEL_FORMAT_BGR24_NON_DIB:
        Convolution_3x3_RGB(&highpass_kernel_3x3[0],
                            static_cast<U8*>(pFrameData),
                            width,
                            height);
        break;
    default:
        return ApiInvalidParameterError;
    }

    return ApiSuccess;
}

template<typename T>
void
SobelFilter_3x3_Impl(T* pData, T* pCopy, int width, int height)
{
    Convolution_3x3<T>( &sobel_vertical_3x3[0],
                        pData,
                        width,
                        height );
    Convolution_3x3<T>( &sobel_horizontal_3x3[0],
                        pCopy,
                        width,
                        height );
    for (int i = 0; i < width*height; i++)
    {
        // Should really be: sqrt(sqr(pData[i])+sqr(pCopy[i])), but this is faster and close enough:
        pData[i] = (pData[i] + pCopy[i]) / 2 ;
    }
}

void
SobelFilter_3x3_12Bit_Packed_Impl(U8* const pData, U8 * const pCopy, const int width, const int height)
{
    Convolution_3x3<U8>( &sobel_vertical_3x3[0],
                        pData,
                        width,
                        height );
    Convolution_3x3<U8>( &sobel_horizontal_3x3[0],
                        pCopy,
                        width,
                        height );
    int widthPlusHalf = width + width/2;
    for (int i = 0; i < widthPlusHalf*height; i++)
    {
        // Should really be: sqrt(sqr(pData[i])+sqr(pCopy[i])), but this is faster and close enough:
        pData[i] = (pData[i] + pCopy[i]) / 2 ;
    }
}

void
SobelFilter_3x3_RGB_Impl(U8* const pData, U8* const pCopy,const int width, const int height)
{
    Convolution_3x3_RGB(&sobel_vertical_3x3[0],
                        pData,
                        width,
                        height );
    Convolution_3x3_RGB(&sobel_horizontal_3x3[0],
                        pCopy,
                        width,
                        height );
    for (int i = 0; i < width*height*3; i++)
    {
        // Should really be: sqrt(sqr(pData[i])+sqr(pCopy[i])), but this is faster and close enough:
        pData[i] = (pData[i] + pCopy[i]) / 2 ;
    }
}

PXLAPI_CALLBACK(PxLCallbackSobel)
{
    int decX = static_cast<int>(pFrameDesc->PixelAddressingValue.fHorizontal);
    int decY = static_cast<int>(pFrameDesc->PixelAddressingValue.fVertical);
    int width = DEC_SIZE(static_cast<int>(pFrameDesc->Roi.fWidth), decX);
    int height = DEC_SIZE(static_cast<int>(pFrameDesc->Roi.fHeight), decY);
    float pixelSize = PxLPixelFormat::bytesPerPixel(static_cast<int>(uDataFormat));
    int bufferSize = static_cast<int> (static_cast<float>(width)
                                       * static_cast<float>(height)
                                       * pixelSize);

    std::vector<U8> buffer(bufferSize);
    memcpy(&buffer[0], pFrameData, bufferSize);

    switch (uDataFormat)
    {
    case PIXEL_FORMAT_MONO8:
    case PIXEL_FORMAT_BAYER8_BGGR:
    case PIXEL_FORMAT_BAYER8_GBRG:
    case PIXEL_FORMAT_BAYER8_GRBG:
    case PIXEL_FORMAT_BAYER8_RGGB:
        SobelFilter_3x3_Impl<U8>(static_cast<U8*>(pFrameData),
                                 static_cast<U8*>(&buffer[0]),
                                 width,
                                 height);
        break;

    case PIXEL_FORMAT_MONO16:
    case PIXEL_FORMAT_BAYER16_BGGR:
    case PIXEL_FORMAT_BAYER16_GBRG:
    case PIXEL_FORMAT_BAYER16_GRBG:
    case PIXEL_FORMAT_BAYER16_RGGB:
        SobelFilter_3x3_Impl<U16>(static_cast<U16*>(pFrameData),
                                  reinterpret_cast<U16*>(&buffer[0]),
                                  width,
                                  height);
        break;

    case PIXEL_FORMAT_MONO12_PACKED:
    case PIXEL_FORMAT_BAYER12_BGGR_PACKED:
    case PIXEL_FORMAT_BAYER12_GBRG_PACKED:
    case PIXEL_FORMAT_BAYER12_GRBG_PACKED:
    case PIXEL_FORMAT_BAYER12_RGGB_PACKED:
        SobelFilter_3x3_12Bit_Packed_Impl(static_cast<U8*>(pFrameData),
                                          reinterpret_cast<U8*>(&buffer[0]),
                                          width,
                                          height);
        break;

    case PIXEL_FORMAT_RGB24:
    case PIXEL_FORMAT_RGB24_NON_DIB:
    case PIXEL_FORMAT_BGR24_NON_DIB:
        SobelFilter_3x3_RGB_Impl(static_cast<U8*>(pFrameData),
                                 static_cast<U8*>(&buffer[0]),
                                 width,
                                 height);
        break;
    default:
        return ApiInvalidParameterError;
    }

    return ApiSuccess;
}


//
// Cheap 'n' Easy
// Quick 'n' Dirty
//
PXLAPI_CALLBACK(PxLCallbackAscii)
{
    const int decX = static_cast<int>(pFrameDesc->PixelAddressingValue.fHorizontal);
    const int decY = static_cast<int>(pFrameDesc->PixelAddressingValue.fVertical);
    const int width = DEC_SIZE(static_cast<int>(pFrameDesc->Roi.fWidth), decX);
    const int height = DEC_SIZE(static_cast<int>(pFrameDesc->Roi.fHeight), decY);

    int asciiChars[][64] = {
        // #
        {
            0,0,0,0,0,0,0,0,
            0,0,1,0,0,1,0,0,
            0,1,1,1,1,1,1,0,
            0,0,1,0,0,1,0,0,
            0,0,1,0,0,1,0,0,
            0,1,1,1,1,1,1,0,
            0,0,1,0,0,1,0,0,
            0,0,0,0,0,0,0,0,
        },
        // '$'
        {
            0,0,0,1,0,0,0,0,
            0,0,0,1,1,0,0,0,
            0,0,1,0,0,0,0,0,
            0,0,1,1,0,0,0,0,
            0,0,0,1,1,0,0,0,
            0,0,0,0,1,0,0,0,
            0,0,1,1,0,0,0,0,
            0,0,0,1,0,0,0,0,
        },
        // 'O'
        {
            0,0,0,0,0,0,0,0,
            0,0,1,1,0,0,0,0,
            0,1,0,0,1,0,0,0,
            1,0,0,0,0,1,0,0,
            1,0,0,0,0,1,0,0,
            0,1,0,0,1,0,0,0,
            0,0,1,1,0,0,0,0,
            0,0,0,0,0,0,0,0,
        },
        // '='
        {
            0,0,0,0,0,0,0,0,
            0,0,0,0,0,0,0,0,
            1,1,1,1,1,1,1,0,
            0,0,0,0,0,0,0,0,
            0,0,0,0,0,0,0,0,
            1,1,1,1,1,1,1,0,
            0,0,0,0,0,0,0,0,
            0,0,0,0,0,0,0,0,
        },
        // '+'
        {
            0,0,0,0,0,0,0,0,
            0,0,0,1,0,0,0,0,
            0,0,0,1,0,0,0,0,
            0,0,0,1,0,0,0,0,
            1,1,1,1,1,1,1,0,
            0,0,0,1,0,0,0,0,
            0,0,0,1,0,0,0,0,
            0,0,0,1,0,0,0,0,
        },
        // '|'
        {
            0,0,0,0,0,0,0,0,
            0,0,0,1,0,0,0,0,
            0,0,0,1,0,0,0,0,
            0,0,0,1,0,0,0,0,
            0,0,0,1,0,0,0,0,
            0,0,0,1,0,0,0,0,
            0,0,0,1,0,0,0,0,
            0,0,0,1,0,0,0,0,
        },
        // '^'
        {
            0,0,0,1,0,0,0,0,
            0,0,1,0,1,0,0,0,
            0,1,0,0,0,1,0,0,
            0,0,0,0,0,0,0,0,
            0,0,0,0,0,0,0,0,
            0,0,0,0,0,0,0,0,
            0,0,0,0,0,0,0,0,
            0,0,0,0,0,0,0,0,
        },
        // '.'
        {
            0,0,0,0,0,0,0,0,
            0,0,0,0,0,0,0,0,
            0,0,0,0,0,0,0,0,
            0,0,0,0,0,0,0,0,
            0,0,0,0,0,0,0,0,
            0,0,0,0,0,0,0,0,
            0,0,0,1,0,0,0,0,
            0,0,0,0,0,0,0,0,
        },
        {
            0,0,0,0,0,0,0,0,
            0,0,0,0,0,0,0,0,
            0,0,0,0,0,0,0,0,
            0,0,0,0,0,0,0,0,
            0,0,0,0,0,0,0,0,
            0,0,0,0,0,0,0,0,
            0,0,0,0,0,0,0,0,
            0,0,0,0,0,0,0,0,
        },

    };

    const int numRows = height / 8;
    const int numCols = width / 8;

    if (uDataFormat == PIXEL_FORMAT_MONO8)
    {
        U8* pFirstPixel = (U8*)pFrameData;
        const int maxTotal = 255 * 64;
        for(int row = 0; row < numRows; row++)
        {
            for(int col = 0; col < numCols; col++)
            {
                // Convert an 8x8 section to an ASCII char
                U8* pAsciiPixel = &pFirstPixel[row*8*width + col*8];
                U8* pPixel = pAsciiPixel;
                int total = 0;
                for(int r = 0; r < 8; r++, pPixel += (width-8))
                {
                    for(int c = 0; c < 8; c++, pPixel++)
                    {
                        total += *pPixel;
                    }
                }
                // Now program in the ascii character
                int asciiCharIndex = (int) (((float)total / (float)maxTotal) * 9);
                if (9 == asciiCharIndex)
                {
                    asciiCharIndex = 8;
                }
                pPixel = pAsciiPixel;
                int bitIndex = 0;
                for(int r = 0; r < 8; r++, pPixel += (width-8))
                {
                    for(int c = 0; c < 8; c++, pPixel++)
                    {
//                      *pPixel = 255 - asciiChars[asciiCharIndex][bitIndex++] * 255;
                        *pPixel = (1 - asciiChars[asciiCharIndex][bitIndex++]) * 255;
                    }
                }
            }
        }
    }
    else if (PIXEL_FORMAT_RGB24 == uDataFormat ||
            PIXEL_FORMAT_RGB24_NON_DIB == uDataFormat ||
            PIXEL_FORMAT_BGR24_NON_DIB == uDataFormat)
    {
        RGBPixel* pFirstPixel = (RGBPixel*)pFrameData;
        const int maxColorTotal = 255 * 64;
        const int maxPixelTotal = maxColorTotal * 3;
        for(int row = 0; row < numRows; row++)
        {
            for(int col = 0; col < numCols; col++)
            {
                // Convert an 8x8 section of RGB24 pixels to an ASCII char
                RGBPixel* pAsciiPixel = &pFirstPixel[row*8*width + col*8];
                RGBPixel* pPixel = pAsciiPixel;
                int totals[3] = { 0, 0, 0};
                for(int r = 0; r < 8; r++, pPixel += (width-8))
                {
                    for(int c = 0; c < 8; c++, pPixel++)
                    {
                        totals[0] += pPixel->R;
                        totals[1] += pPixel->G;
                        totals[2] += pPixel->B;
                    }
                }

                // Calculate the average colour for the region
                int total = 0;
                int colours[3];
                for(int i = 0; i < 3; i++)
                {
                    colours[i] = (int)(255.0f * ((float)totals[i] / (float)maxColorTotal));
                    total += totals[i];
                }

                // Now program in the ascii character
                int asciiCharIndex = (int) (((float)total / (float)maxPixelTotal)* 9);
                if (9 == asciiCharIndex)
                {
                    asciiCharIndex = 8;
                }
                pPixel = pAsciiPixel;
                int bitIndex = 64-8;    // Remember that RGB24 has the rows flipped
                for(int r = 0; r < 8; r++, pPixel += (width-8), bitIndex -= 8*2)
                {
                    for(int c = 0; c < 8; c++, pPixel++, bitIndex++)
                    {
                        int bit = asciiChars[asciiCharIndex][bitIndex];
                        if (bit)
                        {
                            pPixel->R = colours[0];
                            pPixel->G = colours[1];
                            pPixel->B = colours[2];
                        }
                        else
                        {
                            // Colour shows up best against a black background
                            pPixel->R = 0;
                            pPixel->G = 0;
                            pPixel->B = 0;
                        }
                        ;
                    }
                }
            }
        }
    }
    return 0;
}


PXLAPI_CALLBACK(PxLCallbackTreshold50Percent)
{
    const int decX = static_cast<int>(pFrameDesc->PixelAddressingValue.fHorizontal);
    const int decY = static_cast<int>(pFrameDesc->PixelAddressingValue.fVertical);
    const int width = DEC_SIZE(static_cast<int>(pFrameDesc->Roi.fWidth), decX);
    const int height = DEC_SIZE(static_cast<int>(pFrameDesc->Roi.fHeight), decY);
    const int numPixels = width * height;

    if (uDataFormat == PIXEL_FORMAT_MONO8)
    {
        U8* pPixel = (U8*)pFrameData;
        U8* pLastPixel = &pPixel[numPixels];
        while(pPixel != pLastPixel)
        {
            U8 value = *pPixel;
            value = (value < 128) ? 0 : 255;
            *pPixel = value;
            pPixel++;
        }
    }
    else if (uDataFormat == PIXEL_FORMAT_RGB24 ||
             uDataFormat == PIXEL_FORMAT_RGB24_NON_DIB ||
             uDataFormat == PIXEL_FORMAT_BGR24_NON_DIB)
    {
        RGBPixel* pPixel = (RGBPixel*)pFrameData;
        RGBPixel* pLastPixel = &pPixel[numPixels];
        while(pPixel != pLastPixel)
        {
            int total = pPixel->R + pPixel->G + pPixel->B;
            int newValue = (total < 128*3) ? 0 : 255;
            pPixel->R = newValue;
            pPixel->G = newValue;
            pPixel->B = newValue;
            pPixel++;
        }
    }
    else
    {
        return ApiInvalidParameterError;
    }

    return 0;

}


PXLAPI_CALLBACK(PxLCallbackHistogramEqualization)
{
    int decX = static_cast<int>(pFrameDesc->PixelAddressingValue.fHorizontal);
    int decY = static_cast<int>(pFrameDesc->PixelAddressingValue.fVertical);
    int width = DEC_SIZE(static_cast<int>(pFrameDesc->Roi.fWidth), decX);
    int height = DEC_SIZE(static_cast<int>(pFrameDesc->Roi.fHeight), decY);
    int nPixels = width * height;

    if (uDataFormat == PIXEL_FORMAT_MONO8)
    {
        const int NUM_PIXEL_VALUES = 256;
        U8* pData = static_cast<U8*>(pFrameData);
        std::vector<int> map(NUM_PIXEL_VALUES, 0);
        int i;
        for (i = 0; i < nPixels; i++)
        {
            ++map[pData[i]];
        }
        for (i = 1; i < NUM_PIXEL_VALUES; i++)
        {
            map[i] += map[i-1];
        }
        for (i = 0; i < NUM_PIXEL_VALUES; i++)
        {
            map[i] = (NUM_PIXEL_VALUES-1) * map[i] / nPixels;
        }

        for (i = 0; i < nPixels; i++)
        {
            pData[i] = map[pData[i]];
        }
    }
    else if (uDataFormat == PIXEL_FORMAT_RGB24 ||
             uDataFormat == PIXEL_FORMAT_RGB24_NON_DIB ||
             uDataFormat == PIXEL_FORMAT_BGR24_NON_DIB)
    {
        // Equalize the Y part of YUV data, then convert back to RGB.
        // Remember that PIXEL_FORMAT_RGB24 really means that the data is
        // in "BGR" format - that is, the first byte of each triplet is
        // the blue, not the red.
        const int NUM_COLOUR_VALUES = 256;
        U8* pData = static_cast<U8*>(pFrameData);
        std::vector<int> map(NUM_COLOUR_VALUES, 0);
        int i;
        U8 yuv[3] = {0,0,0};

        for (i = 0; i < nPixels; i++)
        {
            if (uDataFormat == PIXEL_FORMAT_RGB24) {
                BGRtoYUV(&pData[3*i], &yuv[0]);
            } else {
                RGBtoYUV(&pData[3*i], &yuv[0]);
            }
            map[yuv[0]]++;
        }

        for (i = 1; i < NUM_COLOUR_VALUES; i++)
        {
            map[i] += map[i-1];
        }

        for (i = 0; i < NUM_COLOUR_VALUES; i++)
        {
            map[i] = (NUM_COLOUR_VALUES-1) * map[i] / nPixels;
        }

        for (i = 0; i < nPixels; i++)
        {
            if (uDataFormat == PIXEL_FORMAT_RGB24) {
                BGRtoYUV(&pData[3*i], &yuv[0]);
                yuv[0] = map[yuv[0]];
                YUVtoBGR(&yuv[0], &pData[3*i]);
            } else {
                RGBtoYUV(&pData[3*i], &yuv[0]);
                yuv[0] = map[yuv[0]];
                YUVtoRGB(&yuv[0], &pData[3*i]);
            }
        }
    }
    else if (uDataFormat == PIXEL_FORMAT_MONO16)
    {
        // We work only with 10 bits.
        // Pixels with more bits will be truncated.
        const int NUM_PIXEL_VALUES = 1024;
        U16* pData = static_cast<U16*>(pFrameData);
        std::vector<int> map(NUM_PIXEL_VALUES, 0);
        int i;
        for (i = 0; i < nPixels; i++)
        {
            ++map[ DCAM16_TO_TENBIT(pData[i]) ];
        }
        for (i = 1; i < NUM_PIXEL_VALUES; i++)
        {
            map[i] += map[i-1];
        }

        for (i = 0; i < NUM_PIXEL_VALUES; i++)
        {
            map[i] = (NUM_PIXEL_VALUES-1) * map[i] / nPixels;
        }
        for (i = 0; i < nPixels; i++)
        {
            pData[i] = TENBIT_TO_DCAM16( map[ DCAM16_TO_TENBIT(pData[i]) ] );
        }
    }
    else if (uDataFormat == PIXEL_FORMAT_MONO12_PACKED)
    {
        // as per Design Notes above -- we only use the MS 8 bits of data
        const int NUM_PIXEL_VALUES = 256;
        int nPixelsPlusHalf = nPixels + nPixels/2;
        U8* pData = static_cast<U8*>(pFrameData);
        std::vector<int> map(NUM_PIXEL_VALUES, 0);
        int i;
        for (i = 0; i < nPixelsPlusHalf; i++)
        {
            if (i % 3 == 1) continue;
            ++map[pData[i]];
        }
        for (i = 1; i < NUM_PIXEL_VALUES; i++)
        {
            map[i] += map[i-1];
        }
        for (i = 0; i < NUM_PIXEL_VALUES; i++)
        {
            map[i] = (NUM_PIXEL_VALUES-1) * map[i] / nPixels;
        }

        for (i = 0; i < nPixelsPlusHalf; i++)
        {
            if (i % 3 == 1) continue;
            pData[i] = map[pData[i]];
        }
    }
    else if (uDataFormat == PIXEL_FORMAT_MONO12_PACKED_MSFIRST)
    {
        // as per Design Notes above -- we only use the MS 8 bits of data
        const int NUM_PIXEL_VALUES = 256;
        int nPixelsPlusHalf = nPixels + nPixels/2;
        U8* pData = static_cast<U8*>(pFrameData);
        std::vector<int> map(NUM_PIXEL_VALUES, 0);
        int i;
        for (i = 0; i < nPixelsPlusHalf; i++)
        {
            if (i % 3 == 2) continue;
            ++map[pData[i]];
        }
        for (i = 1; i < NUM_PIXEL_VALUES; i++)
        {
            map[i] += map[i-1];
        }
        for (i = 0; i < NUM_PIXEL_VALUES; i++)
        {
            map[i] = (NUM_PIXEL_VALUES-1) * map[i] / nPixels;
        }

        for (i = 0; i < nPixelsPlusHalf; i++)
        {
            if (i % 3 == 2) continue;
            pData[i] = map[pData[i]];
        }
    }
    else if (uDataFormat == PIXEL_FORMAT_MONO10_PACKED_MSFIRST)
    {
        // as per Design Notes above -- we only use the MS 8 bits of data
        const int NUM_PIXEL_VALUES = 256;
        int nPixelsPlusQuarter = nPixels + nPixels/4;
        U8* pData = static_cast<U8*>(pFrameData);
        std::vector<int> map(NUM_PIXEL_VALUES, 0);
        int i;
        for (i = 0; i < nPixelsPlusQuarter; i++)
        {
            if (i % 5 == 4) continue;
            ++map[pData[i]];
        }
        for (i = 1; i < NUM_PIXEL_VALUES; i++)
        {
            map[i] += map[i-1];
        }
        for (i = 0; i < NUM_PIXEL_VALUES; i++)
        {
            map[i] = (NUM_PIXEL_VALUES-1) * map[i] / nPixels;
        }

        for (i = 0; i < nPixelsPlusQuarter; i++)
        {
            if (i % 5 == 4) continue;
            pData[i] = map[pData[i]];
        }
    }
    else
    {
        return ApiInvalidParameterError;
    }

    return ApiSuccess;
}

//
// Displaying areas of saturated pixels and pure black pixels, thereby
// highlighting areas that have lost detail because it's too dark or too bright
//
PXLAPI_CALLBACK(PxLCallbackSaturatedAndBlack)
{
    const int decX = static_cast<int>(pFrameDesc->PixelAddressingValue.fHorizontal);
    const int decY = static_cast<int>(pFrameDesc->PixelAddressingValue.fVertical);
    const int width = DEC_SIZE(static_cast<int>(pFrameDesc->Roi.fWidth), decX);
    const int height = DEC_SIZE(static_cast<int>(pFrameDesc->Roi.fHeight), decY);
    const int numPixels = width * height;

    // If this isn't one of the formats we support, we're out of here.
    switch(uDataFormat) {
        case PIXEL_FORMAT_RGB24:
        case PIXEL_FORMAT_RGB24_NON_DIB:
        case PIXEL_FORMAT_BGR24_NON_DIB:
        case PIXEL_FORMAT_MONO8:
            break;
        default:
            return ApiInvalidParameterError;
    }

    unsigned int TOLERANCE = 5; // +- 5 from pure black or pure white.

    if (PIXEL_FORMAT_RGB24 == uDataFormat ||
        PIXEL_FORMAT_RGB24_NON_DIB == uDataFormat ||
        PIXEL_FORMAT_BGR24_NON_DIB == uDataFormat) {
        U8* pPixel = (U8*)pFrameData;
        U8* pLastPixel = &pPixel[(numPixels-1)*3];
        for(; pPixel <= pLastPixel; pPixel+=3) {
            U32 total = pPixel[0] + pPixel[1] + pPixel[2];
            if (total <= (3 * (0x00 + TOLERANCE))) {
                // Make an all- or near-black pixel blue (cold)
                pPixel[0] = PIXEL_FORMAT_RGB24 == uDataFormat ? 0xFF : 0x00; // B, or R
                pPixel[1] = 0x00; // G
                pPixel[2] = PIXEL_FORMAT_RGB24 == uDataFormat ? 0x00 : 0xFF; // R, or B
            } else if (total >= (3 * (0xFF - TOLERANCE))) {
                // Make an all- or near-white pixel red (hot)
                pPixel[0] = PIXEL_FORMAT_RGB24 == uDataFormat ? 0x00 : 0xFF; // B, or R
                pPixel[1] = 0x00; // G
                pPixel[2] = PIXEL_FORMAT_RGB24 == uDataFormat ? 0xFF : 0x00; // R, or B
            }
        }
    } else if (PIXEL_FORMAT_MONO8 == uDataFormat) {
        U8* pPixel = (U8*)pFrameData;
        U8* pLastPixel = &pPixel[numPixels-1];
        for(; pPixel <= pLastPixel; pPixel++) {
            const U8 pixelVal = *pPixel;
            if (0x00 == pixelVal) {
                *pPixel = 0xFF; // Make an all black pixel white
            }
            if (0xFF == pixelVal) {
                *pPixel = 0x00; // Make an all white pixel black
            }
        }
    }

    return ApiSuccess;

}

//
// Displaying areas of saturated pixels and pure black pixels, thereby
// highlighting areas that have lost detail because it's too dark or too bright
//
PXLAPI_CALLBACK(PxLCallbackNegative)
{
    const int decX = static_cast<int>(pFrameDesc->PixelAddressingValue.fHorizontal);
    const int decY = static_cast<int>(pFrameDesc->PixelAddressingValue.fVertical);
    const int width = DEC_SIZE(static_cast<int>(pFrameDesc->Roi.fWidth), decX);
    const int height = DEC_SIZE(static_cast<int>(pFrameDesc->Roi.fHeight), decY);
    const int numPixels = width * height;

    // If this isn't one of the formats we support, we're out of here.
    switch(uDataFormat) {
        case PIXEL_FORMAT_RGB24:
        case PIXEL_FORMAT_RGB24_NON_DIB:
        case PIXEL_FORMAT_BGR24_NON_DIB:
        case PIXEL_FORMAT_MONO8:
            break;
        default:
            return ApiInvalidParameterError;
    }

    if (PIXEL_FORMAT_RGB24 == uDataFormat ||
        PIXEL_FORMAT_RGB24_NON_DIB == uDataFormat ||
        PIXEL_FORMAT_BGR24_NON_DIB == uDataFormat) {
        U8* pPixel = (U8*)pFrameData;
        U8* pLastPixel = &pPixel[(numPixels-1)*3];
        for(; pPixel <= pLastPixel; pPixel+=3) {
            pPixel[0] = 255 - pPixel[0];
            pPixel[1] = 255 - pPixel[1];
            pPixel[2] = 255 - pPixel[2];
        }
    } else if (PIXEL_FORMAT_MONO8 == uDataFormat) {
        U8* pPixel = (U8*)pFrameData;
        U8* pLastPixel = &pPixel[numPixels-1];
        for(; pPixel <= pLastPixel; pPixel++) {
            const U8 pixelVal = *pPixel;
            *pPixel = 255 - pixelVal;
        }
    }

    return ApiSuccess;

}


//
// Display the image as a monochrome
//
PXLAPI_CALLBACK(PxLCallbackGrayscale)
{
    const int decX = static_cast<int>(pFrameDesc->PixelAddressingValue.fHorizontal);
    const int decY = static_cast<int>(pFrameDesc->PixelAddressingValue.fVertical);
    const int width = DEC_SIZE(static_cast<int>(pFrameDesc->Roi.fWidth), decX);
    const int height = DEC_SIZE(static_cast<int>(pFrameDesc->Roi.fHeight), decY);
    const int numPixels = width * height;

    // If this isn't one of the formats we support, we're out of here.
    switch(uDataFormat) {
        case PIXEL_FORMAT_RGB24:
        case PIXEL_FORMAT_RGB24_NON_DIB:
        case PIXEL_FORMAT_BGR24_NON_DIB:
        case PIXEL_FORMAT_MONO8:
            break;
        default:
            return ApiInvalidParameterError;
    }

    if (PIXEL_FORMAT_RGB24 == uDataFormat ||
        PIXEL_FORMAT_RGB24_NON_DIB == uDataFormat ||
        PIXEL_FORMAT_BGR24_NON_DIB == uDataFormat) {
        U8* pPixel = (U8*)pFrameData;
        U8* pLastPixel = &pPixel[(numPixels-1)*3];
        for(; pPixel <= pLastPixel; pPixel+=3) {
            const float b = static_cast<float>(PIXEL_FORMAT_RGB24 == uDataFormat ? pPixel[0] : pPixel[0]);
            const float g = static_cast<float>(pPixel[1]);
            const float r = static_cast<float>(PIXEL_FORMAT_RGB24 == uDataFormat ? pPixel[2] : pPixel[2]);
            const float Y =  (0.2989f * r) + (0.5870f * g) + (0.1140f * b);
            const U8 y = static_cast<U8>(Y);
            pPixel[0] = y;
            pPixel[1] = y;
            pPixel[2] = y;
        }
    }

    return ApiSuccess;

}


//
// A very simple temporal filter that attempts to remove
// a bit of noise from images by comparing the current image to
// the previous image.
//
static U32
TemporalFilterImpl(void* pFrameData, const U32 pixelFormat, FRAME_DESC const * pFrameDesc, const U8 noiseThreshold)
{
    static std::vector<U8>  s_lastFrame;
    const int decX = static_cast<int>(pFrameDesc->PixelAddressingValue.fHorizontal);
    const int decY = static_cast<int>(pFrameDesc->PixelAddressingValue.fVertical);
    const int width = DEC_SIZE(static_cast<int>(pFrameDesc->Roi.fWidth), decX);
    const int height = DEC_SIZE(static_cast<int>(pFrameDesc->Roi.fHeight), decY);
    const int numPixels = width * height;
    const float pixelSize = PxLPixelFormat::bytesPerPixel(pixelFormat);
    const int frameSize = static_cast<int>( static_cast<float>(numPixels) * pixelSize);

    // Is this the first frame?
    if ((int)s_lastFrame.size() != frameSize) {
        s_lastFrame.clear();
        s_lastFrame.resize(frameSize);
        memcpy(&s_lastFrame[0], (U8*)pFrameData, frameSize);
        return ApiSuccess;
    }

    // If this isn't one of the formats we support, we're out of here.
    switch(pixelFormat) {
        case PIXEL_FORMAT_RGB24:
        case PIXEL_FORMAT_RGB24_NON_DIB:
        case PIXEL_FORMAT_BGR24_NON_DIB:
        case PIXEL_FORMAT_MONO8:
            break;
        default:
            return ApiInvalidParameterError;
    }

    // Go through the pixel values 1 by 1 looking for differences.
    // If the difference is greater than our noise threshold, use the new value and store it in the last frame.
    // If the difference is less than our noise threshold, use the old value.
    U8* pNewPixel       = (U8*)pFrameData;
    U8* pOldPixel       = &s_lastFrame[0];
    U8* pLastOldPixel   = &s_lastFrame[s_lastFrame.size()-1];

    for( ; pOldPixel <= pLastOldPixel ; pNewPixel++, pOldPixel++) {
        U16 newPixel = (U16)*pNewPixel;
        U16 oldPixel = (U16)*pOldPixel;
        U16 delta = abs(newPixel - oldPixel);
        if (delta < noiseThreshold) {
            *pNewPixel = (U8)oldPixel;
        } else {
            *pOldPixel = (U8)newPixel;
        }
    }
    return ApiSuccess;
}

//
// A very simple temporal filter that shows areas that are changing rapidly.
//
PXLAPI_CALLBACK(PxLCallbackMotionDetector)
{
    static std::vector<U8>  s_lastFrame;
    const int decX = static_cast<int>(pFrameDesc->PixelAddressingValue.fHorizontal);
    const int decY = static_cast<int>(pFrameDesc->PixelAddressingValue.fVertical);
    const int width = DEC_SIZE(static_cast<int>(pFrameDesc->Roi.fWidth), decX);
    const int height = DEC_SIZE(static_cast<int>(pFrameDesc->Roi.fHeight), decY);
    const int numPixels = width * height;
    const float pixelSize = PxLPixelFormat::bytesPerPixel(uDataFormat);
    const int frameSize = static_cast<int>(static_cast<float>(numPixels) * pixelSize);

    // Is this the first frame?
    if ((int)s_lastFrame.size() != frameSize) {
        s_lastFrame.clear();
        s_lastFrame.resize(frameSize);
        memcpy(&s_lastFrame[0], (U8*)pFrameData, frameSize);
        return ApiSuccess;
    }

    // If this isn't one of the formats we support, we're out of here.
    switch(uDataFormat) {
        case PIXEL_FORMAT_RGB24:
        case PIXEL_FORMAT_RGB24_NON_DIB:
        case PIXEL_FORMAT_BGR24_NON_DIB:
        case PIXEL_FORMAT_MONO8:
            break;
        default:
            return ApiInvalidParameterError;
    }

    // Go through the pixel values 1 by 1 looking for differences.
    U8* pNewPixel       = (U8*)pFrameData;
    U8* pOldPixel       = &s_lastFrame[0];
    U8* pLastOldPixel   = &s_lastFrame[s_lastFrame.size()-1];

    U32 numPixelsMoving =0;

    for( ; pOldPixel <= pLastOldPixel ; pNewPixel++, pOldPixel++) {
        U16 newPixel = (U16)*pNewPixel;
        U16 oldPixel = (U16)*pOldPixel;
        U16 delta = abs(newPixel - oldPixel);
        if (delta > 64) {
            *pOldPixel = (U8)newPixel;
            *pNewPixel = (U8)0xFF;
            numPixelsMoving++;
        } else {
            *pNewPixel = 0x00;
        }
    }

    return ApiSuccess;
}


PXLAPI_CALLBACK(PxLCallbackTemporalTheshold)
{
    return TemporalFilterImpl(pFrameData, uDataFormat, pFrameDesc, 5);
}


//
// Merges an image with a bitmap file.
//
PXLAPI_CALLBACK(PxLCallbackBitmapOverlay)
{
    const int decX = static_cast<int>(pFrameDesc->PixelAddressingValue.fHorizontal);
    const int decY = static_cast<int>(pFrameDesc->PixelAddressingValue.fVertical);
    int width = DEC_SIZE(static_cast<int>(pFrameDesc->Roi.fWidth), decX);
    int height = DEC_SIZE(static_cast<int>(pFrameDesc->Roi.fHeight), decY);

    // If this isn't one of the formats we support, we're out of here.
    switch(uDataFormat) {
        case PIXEL_FORMAT_RGB24:
        case PIXEL_FORMAT_RGB24_NON_DIB:
        case PIXEL_FORMAT_BGR24_NON_DIB:
        case PIXEL_FORMAT_MONO8:
            break;
        default:
            return ApiInvalidParameterError;
    }

    SDL_Surface* pSurface = (SDL_Surface*)pContext;
    if (! pSurface) return ApiSuccess;  // Callback cancelled -- quietly return

    // pSrc is the bitmap overlay; pDest is the preview buffer
    U8* pSrc = (U8*)pSurface->pixels;
    U8* pDest = (U8*)pFrameData;
    int srcPitch = pSurface->pitch;
    int destPitch = width * sizeof (RGBPixel);
    RGBPixel* pSrcRow;
    RGBPixel* pDestRow;

    // Limit our width and height to the intersections of the two images
    width = min (pSurface->w, width);
    height = min (pSurface->h, height);

    for (int y=0; y< height; y++)
    {
        pSrcRow = (RGBPixel*)pSrc;
        pDestRow = (RGBPixel*)pDest;
        for (int x=0; x<width; x++)
        {
            // If the bitmap pixel is not 'white', then replace the pixel in the image with the one from
            // the bitmap
            if (pSrcRow[x].R != 255 || pSrcRow[x].G != 255 || pSrcRow[x].B != 255)
            {
                // bitmaps are 'unusual', in that the color channels are represented in the order of
                // B-G-R, not the normal R-G-B.  So, we can't simply copy the three bytes over as you might
                // expect; we need to assign the individual color channels
                //pDestRow[x] = pSrcRow[x];
                pDestRow[x].R = pSrcRow[x].B;
                pDestRow[x].G = pSrcRow[x].G;
                pDestRow[x].B = pSrcRow[x].R;
            }
        }
        pSrc += srcPitch;
        pDest += destPitch;
    }

    return ApiSuccess;
}

//
// A very simple filter that draws a cross hair in the middle of the image.
//
PXLAPI_CALLBACK(PxLCallbackCrosshairOverlay)
{
    const int decX = static_cast<int>(pFrameDesc->PixelAddressingValue.fHorizontal);
    const int decY = static_cast<int>(pFrameDesc->PixelAddressingValue.fVertical);
    const int width = DEC_SIZE(static_cast<int>(pFrameDesc->Roi.fWidth), decX);
    const int height = DEC_SIZE(static_cast<int>(pFrameDesc->Roi.fHeight), decY);

    // If this isn't one of the formats we support, we're out of here.
    switch(uDataFormat) {
        case PIXEL_FORMAT_RGB24:
        case PIXEL_FORMAT_RGB24_NON_DIB:
        case PIXEL_FORMAT_BGR24_NON_DIB:
        case PIXEL_FORMAT_MONO8:
            break;
        default:
            return ApiInvalidParameterError;
    }

    // The Cross hair will be red lines (3 pixels wide), and the 10% of the size of the image
    int lengthX = width / 10;
    int lengthY = height / 10;
    if (PIXEL_FORMAT_RGB24 == uDataFormat ||
        PIXEL_FORMAT_RGB24_NON_DIB == uDataFormat ||
        PIXEL_FORMAT_BGR24_NON_DIB == uDataFormat) {
        // Vertical line
        RGBPixel* pFirstPixel = (RGBPixel*)pFrameData;
        RGBPixel* pPixel = &pFirstPixel[(height/2 - lengthY/2)*width + width/2 ];
        for(int i = 0; i < lengthY; i++)
        {
            pPixel[width*i].R = 255; pPixel[width*i].G = 0; pPixel[width*i].B = 0;
            pPixel[width*i - 1].R = 255; pPixel[width*i - 1].G = 0; pPixel[width*i - 1].B = 0;
            pPixel[width*i + 1].R = 255; pPixel[width*i + 1].G = 0; pPixel[width*i + 1].B = 0;
        }
        // Horizontal line
        pPixel = &pFirstPixel[((height/2 -1)*width) + width/2 - lengthX/2];
        for(int i = 0; i < lengthX; i++)
        {
            pPixel[i].R = 255; pPixel[i].G = 0; pPixel[i].B = 0;
            pPixel[width + i].R = 255; pPixel[width + i].G = 0; pPixel[width + i].B = 0;
            pPixel[2*width + i].R = 255; pPixel[2*width + i].G = 0; pPixel[2*width + i].B = 0;
        }
    } else if (PIXEL_FORMAT_MONO8 == uDataFormat) {
        U8* pFirstPixel = (U8*)pFrameData;
        U8* pPixel = &pFirstPixel[(height/2 - lengthY/2)*width + width/2 ];
        for(int i = 0; i < lengthY; i++)
        {
            pPixel[width*i] = 255;
            pPixel[width*i - 1] = 255;;
            pPixel[width*i + 1] = 255;
        }
        pPixel = &pFirstPixel[((height/2 -1)*width) + lengthX/2];
        for(int i = 0; i < lengthX; i++)
        {
            pPixel[i] = 255;
            pPixel[width + i] = 255;
            pPixel[2*width + i] = 255;
        }
    }

    return ApiSuccess;
}








