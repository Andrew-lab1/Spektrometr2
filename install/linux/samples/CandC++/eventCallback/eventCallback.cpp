//
// eventCallback.cpp
//
// Demonstrates how to use event callbacks 
//
#include "PixeLINKApi.h"
#include "LinuxUtil.h"

#include <unistd.h>
#include <stdio.h>
#include <vector>

#define STALL_TIME (20) // 20 seconds

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Callback function called by the API with each event (of interest) reported by the camera. 
//
// N.B. This is called by the API on a thread created in the API. 
//

static U32
EventCallbackFunc(
	HANDLE		hCamera,
    U32         eventId,
    double      eventTimestamp,
    U32         numDataBytes,
	LPVOID		pData,
	LPVOID		userData)
{
   printf("EventCallbackFunction: hCamera=0x%p, eventId=%d\n", hCamera, eventId);
	printf("    eventTimestamp=%f, numDataBytes=%d\n", eventTimestamp, numDataBytes);
	printf("    pData=0x%p, userData=0xp\n\n", pData, userData);
	return ApiSuccess;
}

int 
main(int argc, char* argv[])
{

    printf("This sample application will use the GPI line to demostrate events, OK to proceed Y/N? ");
    char userChar = getchar ();
    if (userChar != 'y' && userChar != 'Y') {
        return EXIT_SUCCESS;
    }

   HANDLE hCamera;
	PXL_RETURN_CODE rc = PxLInitialize(0, &hCamera);
	if (!(API_SUCCESS(rc))) {
		printf("ERROR on PxLInitialize: 0x%8.8X\n", rc);
		return EXIT_FAILURE;
	}

   printf("\n\nMain thread, stalling for %d seconds awaiting events.  Toggle the GPI line...\n\n", STALL_TIME);

	U32 userData = 0x5AFECAFE;
	rc = PxLSetEventCallback(
		hCamera,	
		EVENT_ANY, // We can specify a specific event, or all of them
		(LPVOID)&userData,
		EventCallbackFunc);
	if (!API_SUCCESS(rc)) {
        ERROR_REPORT err;
        PxLGetErrorReport (hCamera, &err);
		  printf("ERROR setting event callback function: 0x%8.8X (%s)\n", rc, err.strReturnCode);
        PxLUninitialize (hCamera);
		return EXIT_FAILURE;
	}

   sleep (STALL_TIME);

   PxLSetEventCallback (hCamera, EVENT_ANY, NULL, NULL); // Cancel the callback
	PxLUninitialize(hCamera);
	return EXIT_SUCCESS;
}


