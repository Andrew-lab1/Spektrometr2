//
// getsnapshot.h
//
#ifndef GETSNAPSHOT_H
#define GETSNAPSHOT_H

#include <PixeLINKApi.h>

// Local macros for return values
#ifndef SUCCESS
#define SUCCESS (0)
#endif
#ifndef FAILURE
#define FAILURE (1)
#endif

int	GetSnapshot(HANDLE hCamera, U32 imageFormat, const char* pFilename);
U32	DetermineRawImageSize(HANDLE hCamera);
U32	GetPixelSize(U32 pixelFormat);
int	GetRawImage(HANDLE  hCamera, char* pRawImage, U32 rawImageSize, FRAME_DESC* pFrameDesc);
int	EncodeRawImage(const char*, const FRAME_DESC*, U32, char**, U32*);
int	SaveImageToFile(const char* pFilename, const char* pImage, U32 imageSize);
PXL_RETURN_CODE		GetNextFrame(HANDLE hCamera, U32 bufferSize, void* pFrame, FRAME_DESC* pFrameDesc);


#endif