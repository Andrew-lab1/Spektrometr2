//
// A demonstration of how to enable jumbo frames for a Gigabit Ethernet camera.
//
// NOTE:
// - All GigE cameras initialized after jumbo frames are enabled will use jumbo frames.
// - All GigE cameras initialized after jumbo frames are disabled will not use jumbo frames.
// - Support for jumbo frames is limited to the life of the application. i.e. You must enable it
//   each time your program runs.
// - Support for jumbo frames is limited to the life of the application enabling jumbo frames. i.e. If you have
//   another program talking to a PixeLINK GigE camera, that application must itself enable jumbo frames.
//
// BE AWARE:
// - The IEEE 802 standards committee does not official recognize jumbo frames.
// 
// - By "enabling" jumbo frames here, the PixeLINK library is simply being asked to use jumbo frames. Your 
//   network interface card (NIC) must not only support jumbo frames, but must also have jumbo frames enabled.
// 
// - NICs that do support jumbo frames typically default to jumbo frame support DISABLED. Please consult the 
//   documentation for your NIC.
//
// - All devices (routers, switches etc.) between the host NIC and the GigE camera must support and have 
//   jumbo frames enabled.
//
#include "PixeLINKApi.h"

#include <unistd.h>
#include <iostream>

//
// Some of the commands supported by PxLPrivateCmd()
// These are just the jumbo frame-specific ones.
//
enum {
	PRIVATE_COMMAND_ENABLE_JUMBO_FRAMES    = 37,
	PRIVATE_COMMAND_DISABLE_JUMBO_FRAMES   = 38,
};


//
// Enable or disable jumbo frame support in the PixeLINK API.
//
U32
EnableJumboFrames(int enable)
{
	const HANDLE hCamera = NULL;
	U32 opcode = (enable) ? PRIVATE_COMMAND_ENABLE_JUMBO_FRAMES : PRIVATE_COMMAND_DISABLE_JUMBO_FRAMES;
	const PXL_RETURN_CODE rc = PxLPrivateCmd(hCamera, sizeof(opcode), &opcode);
	return rc;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

int 
main(int argc, char* argv[])
{
	// Enable jumbo frames before we connect to any GigE cameras
	PXL_RETURN_CODE rc = EnableJumboFrames(true);
	if (!API_SUCCESS(rc)) {
		std::cout << "ERROR: Unable to enable jumbo frames. Error code 0x" << std::hex << rc << ".";
		return 1;
	}

	// And now any GigE cameras initialized after this will use jumbo frames.
	// Just connecting to a camera (any camera) 
	HANDLE hCamera;
	rc = PxLInitialize(0, &hCamera);
	if (!API_SUCCESS(rc)) {
		std::cout << "ERROR: Unable to initialize a camera. Error code 0x" << std::hex << rc << ".";
		return 1;
	}

	// Do camera stuff

	PxLUninitialize(hCamera);

	return 0;
}


