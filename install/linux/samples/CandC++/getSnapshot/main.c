//
// main.c
//
// Sample code to capture an image from a PixeLINK camera and save 
// the encoded image to a file.
//

#include <stdio.h>
#include <PixeLINKApi.h>
#include "getsnapshot.h"

int 
main(int argc, char* argv[])
{
	HANDLE hCamera;
	const char* pFilenameJpg = "snapshot.jpg";
	const char* pFilenameBmp = "snapshot.bmp";
	const char* pFilenameTiff = "snapshot.tiff";
	const char* pFilenamePsd = "snapshot.psd";
	const char* pFilenameRgb24 = "snapshot.rgb24.bin";
	const char* pFilenameRgb24Nondib = "snapshot.rgb24nondib.bin";
	const char* pFilenameRgb48 = "snapshot.rgb48.bin";
	const char* pFilenameMono8 = "snapshot.mono8.bin";
	int retVal;

	// Tell the camera we want to start using it.
	// NOTE: We're assuming there's only one camera.
	if (!API_SUCCESS(PxLInitialize(0, &hCamera))) {
		return 1;
	}

	// Get the snapshots and save it to a file
	retVal = GetSnapshot(hCamera, IMAGE_FORMAT_JPEG, pFilenameJpg);
	if (SUCCESS == retVal) {
		printf("Saved image to '%s'\n", pFilenameJpg);
		retVal = GetSnapshot(hCamera, IMAGE_FORMAT_BMP, pFilenameBmp);
		if (SUCCESS == retVal) {
			printf("Saved image to '%s'\n", pFilenameBmp);
			retVal = GetSnapshot(hCamera, IMAGE_FORMAT_TIFF, pFilenameTiff);
			if (SUCCESS == retVal) {
				printf("Saved image to '%s'\n", pFilenameTiff);
				retVal = GetSnapshot(hCamera, IMAGE_FORMAT_PSD, pFilenamePsd);
				if (SUCCESS == retVal) {
					printf("Saved image to '%s'\n", pFilenamePsd);
				        retVal = GetSnapshot(hCamera, IMAGE_FORMAT_RAW_RGB24, pFilenameRgb24);
					if (SUCCESS == retVal) {
						printf("Saved image to '%s'\n", pFilenameRgb24);
						retVal = GetSnapshot(hCamera, IMAGE_FORMAT_RAW_RGB24_NON_DIB, pFilenameRgb24Nondib);
						if (SUCCESS == retVal) {
							printf("Saved image to '%s'\n", pFilenameRgb24Nondib);
							retVal = GetSnapshot(hCamera, IMAGE_FORMAT_RAW_RGB48, pFilenameRgb48);
							if (SUCCESS == retVal) {
								printf("Saved image to '%s'\n", pFilenameRgb48);
								// Only capture MONO8 for monochrome cameras
								/*
								retVal = GetSnapshot(hCamera, IMAGE_FORMAT_RAW_MONO8, pFilenameMono8);
								if (SUCCESS == retVal) {
									printf("Saved image to '%s'\n", pFilenameMono8);
								}
								*/
							}
						}
					}
				}
			}
		}
	}
								retVal = GetSnapshot(hCamera, IMAGE_FORMAT_RAW_MONO8, pFilenameMono8);
								if (SUCCESS == retVal) {
									printf("Saved image to '%s'\n", pFilenameMono8);
								}
	if (SUCCESS != retVal) {
		printf("ERROR: Unable to capture an image\n");
	}

	// Tell the camera we're done with it.
	PxLUninitialize(hCamera);

	return (SUCCESS == retVal) ? 0 : 1;
}
