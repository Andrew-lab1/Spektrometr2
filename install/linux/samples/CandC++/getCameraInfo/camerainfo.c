/*
 * camerainfo.c
 *
 * Demonstration of a trivial interaction with the PixeLINK API.
 *
 * This demo program has minimal error handling, as its purpose is 
 * to show minimal code to interact with the PixeLINK API, not
 * tell you how to do your error handling.
 *
 * With this program, we assume that there is at least one camera
 * connected, and that no cameras are connected or disconnected 
 * while the program is running.
 */

#include "camerainfo.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>

/*
 * This function assumes there's only one camera connected.
 */
void
printInfoForOneCamera(void)
{
	HANDLE hCamera;
	CAMERA_INFO cameraInfo;
	PXL_RETURN_CODE rc;

	rc = PxLInitialize(0, &hCamera);
	if (API_SUCCESS(rc)) {
		rc = PxLGetCameraInfoEx(hCamera, &cameraInfo, sizeof(CAMERA_INFO));
		if (API_SUCCESS(rc)) {
			printCameraInfo(&cameraInfo);
		}

		PxLUninitialize(hCamera);
	}

}

/*
 * This will print information for all the cameras connected and connectable.
 */
void 
printInfoForAllCameras(void)
{
	PXL_RETURN_CODE rc;
	U32 numCameras = 0;
	/* First: Determine how many cameras are connected and available for connecting */
	rc = PxLGetNumberCameras(NULL, &numCameras);
	if (API_SUCCESS(rc)) {
		if(numCameras > 0) {
			/* Malloc an array to hold the serial numbers */
			U32* pSerialNumbers = (U32*)malloc(sizeof(U32) * numCameras);
			if (NULL != pSerialNumbers) {
				/* Read in all the serial numbers */
				rc = PxLGetNumberCameras(&pSerialNumbers[0], &numCameras);
				if (API_SUCCESS(rc)) {
					unsigned int i;
					/* One-by-one, get the camera info for each camera */
					for(i = 0; i < numCameras; i++) {
						HANDLE hCamera;
						/* Connect to the camera */
						rc = PxLInitialize(pSerialNumbers[i], &hCamera);
						if (API_SUCCESS(rc)) {
							CAMERA_INFO cameraInfo;
							/* And get the info */
							rc = PxLGetCameraInfoEx(hCamera, &cameraInfo, sizeof(CAMERA_INFO));
							if (API_SUCCESS(rc)) {
								printf("\nCamera %d of %d (serialNumber %d):\n", i+1, numCameras, pSerialNumbers[i]);
								printCameraInfo(&cameraInfo);
							}
							/*
							 * Don't forget to tell the camera we're done
							 * so that we don't any camera-related resources.
							 */
							PxLUninitialize(hCamera);
						}
					}
				}
			}
			free(pSerialNumbers);
			pSerialNumbers = 0;
		}
	}
	return;
}

/*
 *
 * Print all the info in the CAMERA_INFO struct 
 *
 */
void 
printCameraInfo(const CAMERA_INFO* pCameraInfo)
{
	assert(NULL != pCameraInfo);

	printf("Name -------------- \'%s\'\n", pCameraInfo->CameraName);
	printf("Description ------- \'%s\'\n", pCameraInfo->Description);
	printf("Firmware Version -- \'%s\'\n", pCameraInfo->FirmwareVersion);
	printf("Bootload Version -- \'%s\'\n", pCameraInfo->BootloadVersion);
	printf("FPGA Version ------ \'%s\'\n", pCameraInfo->FPGAVersion);
	printf("XML Version ------- \'%s\'\n", pCameraInfo->XMLVersion);
	printf("Model Name -------- \'%s\'\n", pCameraInfo->ModelName);
	printf("Serial Number ----- \'%s\'\n", pCameraInfo->SerialNumber);
	printf("Vendor Name ------- \'%s\'\n", pCameraInfo->VendorName);
}

