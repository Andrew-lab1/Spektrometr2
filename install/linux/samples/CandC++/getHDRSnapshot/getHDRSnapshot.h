//
// gethdrsnapshot.h
//
#ifndef getHDRSnapshot_H
#define getHDRSnapshot_H

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

bool DoesCameraSupportHdr(HANDLE hCamera);
int SetHdrMode(HANDLE hCamera, U32 hdrMode);
char* PrepareFileName(const char* pFileRoot, const char* pEnding);
int GetHdrSnapshot(HANDLE hCamera, U32 imageFormat, const char* pFileName);
U32 DetermineRawImageSize(HANDLE hCamera);
F32 GetPixelSize(U32 pixelFormat);
int GetRawImage(HANDLE  hCamera, char* pRawImage, U32 rawImageSize, FRAME_DESC* pFrameDesc);
PXL_RETURN_CODE GetNextFrame(HANDLE hCamera, U32 bufferSize, void* pFrame, FRAME_DESC* pFrameDesc);
int EncodeRawImage(const char*, const FRAME_DESC*, U32, char**, U32*);
int SaveImageToFile(const char* pFileName, const char* pImage, U32 imageSize);

#endif
