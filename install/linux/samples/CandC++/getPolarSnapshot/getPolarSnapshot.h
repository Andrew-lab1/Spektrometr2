//
// getPolarSnapshot.h
//
#ifndef getPolarSnapshot_H
#define getPolarSnapshot_H

#include <PixeLINKApi.h>
#include <stdbool.h>
#include <stdlib.h>

// Local macros for return values
#ifndef SUCCESS
#define SUCCESS (0)
#endif
#ifndef FAILURE
#define FAILURE (1)
#endif

bool IsPolarCamera(HANDLE hCamera);
int SetPolarPixelFormat(HANDLE hCamera);
char* PrepareFileName(const char* pFileRoot, const char* pEnding);
int GetPolarSnapshot(HANDLE hCamera, U32 imageFormat, const char* pFileName, U32 polarChannel);
int SetPolarWeightings(HANDLE hCamera, U32 polWeight0, U32 polWeight45, U32 polWeight90, U32 polWieght135);
U32 DetermineRawImageSize(HANDLE hCamera);
U32 GetPixelSize(U32 pixelFormat);
int GetRawImage(HANDLE hCamera, char* pRawImage, U32 rawImageSize, FRAME_DESC* pFrameDesc);
PXL_RETURN_CODE GetNextFrame(HANDLE hCamera, U32 bufferSize, void* pFrame, FRAME_DESC* pFrameDesc);
int EncodeRawImage(const char*, const FRAME_DESC*, U32, char**, U32*);
int SaveImageToFile(const char* pFileName, const char* pImage, U32 imageSize);

#endif
