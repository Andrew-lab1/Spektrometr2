//
// As required by the GigE Vision specification, PixeLINK GigE cameras use a 'heartbeat'
// between the camera and the host to determine the 'liveness' of the connection. If
// no communication (normal camera interaction, or heartbeat) occurs within a certain
// amount of time, the camera deems the connection as broken, and any further interaction
// (except for connections) is refused.
//
//
// This poses a problem when debugging because the debugger (e.g. Visual Studio, WinDbg)
// will suspend all the threads in a debugee process (i.e. your program which is trying
// to interact with the camera) for various reasons (e.g. breakpoints, exceptions, ...),
// thereby disabling the background heartbeat mechanism, and thereby causing a connection
// to a GigE camera to be terminated by the camera. 
//
// Simply put: debugging a program talking to a PixeLINK GigE camera will have problems
// after hitting a breakpoint or single-stepping.
//
// The only way to improve this situation is to increase the heartbeat timeout. The
// current default is 500ms, but this can currently be increased to as much as 65.535
// seconds. A C/C++ demo application, viz. gigeheartbeat, demonstrates how to use the
// PixeLINK API to do this. This functionality is available with PixeLINK API 7.14 and
// later.
//
// All processes will start by using the default heartbeat timeout. If the heartbeat
// timeout value is changed programmatically, the heartbeat timeout value is valid only
// for the current process, and only for the life of the current process. 
//
// The ability to change the heartbeat timeout is a double-edged sword. If you abruptly
// end your debugging session without giving the PixeLINK API a chance to close the
// connection (i.e. PxLUninitialize), the camera will not allow connections until the
// timeout from the last heartbeat passes. For example, if you set the heartbeat to
// 30 seconds, and then while debugging terminate debugging (e.g. in Visual Studio,
// using Debug\Stop Debugging), without having called PxLUninitialize, it may take up
// to 30 seconds before the camera will accept new connections.
//
#include "PixeLINKApi.h"

#include <unistd.h>
#include <stdio.h>

// Local function prototypes
static PXL_RETURN_CODE	InitializeCamera(U32 serialNumber, HANDLE* phCamera);

// Commands for PxLPrivateCmd
#define PRIVATE_COMMAND_SET_HEARTBEAT_TIMEOUT  40

int 
main(int argc, char* argv[])
{

	HANDLE hCamera = 0;
	PXL_RETURN_CODE rc = InitializeCamera(0, &hCamera);
	if (!API_SUCCESS(rc)) {
		printf("ERROR: Unable to initialize a camera (Return code  = 0x%8.8X)\n",rc);
		return 1;
	}

	//
	// Interact with your GigE camera as usual
	//

	// And now uninitialize in the usual way.
	PxLUninitialize(hCamera);
	hCamera = 0;

	return 0;
}

//
// Before connecting to a camera, set the GigE heartbeat timeout
// so that we can debug this process without killing the connection.
//
// We'll only do this for Debug builds, under the assumption we won't 
// be debugging a Release build under a debugger.
//
//
static PXL_RETURN_CODE
InitializeCamera(U32 serialNumber, HANDLE* phCamera)
{
#ifdef _DEBUG
	//
	// Before connecting to a GigE camera, set the GigE heartbeat timeout.
	// It is recommended that you do this just before calling PxLInitialize. 
	//
	U32 data[2] = {	PRIVATE_COMMAND_SET_HEARTBEAT_TIMEOUT, 0xFFFF }; // Set to max currently supported value (65535 ms = 65.535 seconds)

	PXL_RETURN_CODE rc = PxLPrivateCmd(
		0,	// camera handle doesn't matter
		sizeof(data),
		&data[0]);
	if (!API_SUCCESS(rc)) {
		return rc;
	}
#endif

	// Can now connect in the usual way to the GigE camera.
	return PxLInitialize(serialNumber, phCamera);
}



