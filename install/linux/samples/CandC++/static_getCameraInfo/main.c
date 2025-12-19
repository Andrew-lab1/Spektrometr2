/*
 * main.c
 *
 * Demonstration of a trivial interaction with the PixeLINK API
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

int 
main(int argc, char* argv[])
{
	/* We assume there's only one camera */
	printInfoForOneCamera();

	/*
	 *  You can uncomment this to see how
	 *  to get information for all cameras 
	 */
	/* printInfoForAllCameras(); */

	return 0;
}
