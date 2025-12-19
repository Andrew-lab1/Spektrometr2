//
// This demonstration application assumes that you have one GigE camera visible to the host, and that
// that GigE camera is connected to a GigE card with a statically assigned IP address.
//
// This demo app is incomplete in that we can't know a priori what IP address, subnet mask and 
// gateway *YOU* need to set. If you're unsure what values these need to be, please consult 
// your local network administrator/administratrix.
//
#include "PixeLINKApi.h"

#include <unistd.h>
#include <stdio.h>

#include <assert.h>
#define	ASSERT(x)				do { assert((x)); } while(0)

#define A_OK          0  // non-zero error codes
#define GENERAL_ERROR 1

int 
main(int argc, char* argv[])
{
	U32 numberOfCameras = 0;
	CAMERA_ID_INFO	cameraIdInfo;
	PXL_RETURN_CODE rc;

	// *******************   NOTE: Assign your values here  *******************
	PXL_IP_ADDRESS	cameraIpAddress  =		{  192,  168,    1,    2 };
	PXL_IP_ADDRESS	cameraSubnetMask =		{ 0xFF, 0xFF, 0xFF, 0x00 };
	PXL_IP_ADDRESS	cameraDefaultGateway =	{  222,    1,    1,    1 };
	BOOL32			ipAddressIsPersistent = FALSE;
	// *******************   NOTE: Assign your values here  *******************

	// Remove this after you've set up your own appropriate values above.
	printf ("This demonstration application has not been configured for your local environment\nSee the notes in setIpAddress.c for more information.\n");
	return GENERAL_ERROR;
	

	// Check our assumption that there's only one camera
	rc = PxLGetNumberCamerasEx(NULL, &numberOfCameras);
	ASSERT(API_SUCCESS(rc));
	ASSERT(1 == numberOfCameras);

	// Get the information for that camera
	cameraIdInfo.StructSize = sizeof(cameraIdInfo);
	rc = PxLGetNumberCamerasEx(&cameraIdInfo, &numberOfCameras);
	ASSERT(API_SUCCESS(rc));
	ASSERT(1 == numberOfCameras);

	// A bit of sanity checking
	ASSERT(cameraIdInfo.NicIpAddress.U32Address != 0);

	if ((cameraIpAddress.U32Address & cameraSubnetMask.U32Address) != 
		(cameraIdInfo.NicIpAddress.U32Address & cameraIdInfo.NicIpMask.U32Address)) {
		printf("WARNING: You are setting an IP address that doesn't match the network interface card (NIC)\n");
	}

	rc = PxLSetCameraIpAddress(
        &cameraIdInfo.CameraMac,
        &cameraIpAddress,
		  &cameraSubnetMask,
		  &cameraDefaultGateway,
		  ipAddressIsPersistent);

	printf("PxLSetCameraIpAddress returned 0x%8.8X\n", rc);
	if (!API_SUCCESS(rc)) {
		return GENERAL_ERROR;
	}

	return A_OK;
}


