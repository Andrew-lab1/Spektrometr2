//
// Little utility program that will commit a cameras settings, to
// non volatile memory on the camera.  Subsequently, the camera will use these
// same settings from a power-up.
//
// This same program can be used to commit the cameras current settings, or the
// factory default settings
//
// NOTE: This application assumes there is at most, one PixeLINK camera connected to the system

#include "PixeLINKApi.h"
#include "LinuxUtil.h"

#include <iostream>
#include <stdio.h>
#include <stdexcept>
#include <unistd.h>
#include <stdlib.h>
#include <cassert>
#include <string.h>

using namespace std;

//
// A few useful defines and enums.
//
#define ASSERT(x)	do { assert((x)); } while(0)
#define A_OK 0  // non-zero error codes
#define OPPS 1  // non-specific error code

// Prototypes to allow top-down design
int getAKeystroke();
int   setRunOptions (int argc, char* argv[]);
void  usage (char* argv[]);

// run options
static bool  runOption_useFactoryDefaults = false;

int main(int argc, char* argv[]) {

    HANDLE myCamera;

    ULONG rc = A_OK;

    int keyPressed;

    //
    // Step 1
    //      Determine user options
    if (setRunOptions (argc, argv) != A_OK) {
        usage(argv);
        return OPPS;
    }

    //
    // Step 2
    //      Find, and initialize, a camera
    rc = PxLInitializeEx (0,&myCamera,0);
    if (!API_SUCCESS(rc))
    {
        printf ("Could not Initialize the camera!  Rc = 0x%X\n", rc);
        return OPPS;
    }

    printf ("\n");
    if (runOption_useFactoryDefaults) {
        printf ("WARNING: This application will commit the cameras factory default settings\n");
        printf ("so that they wil be used as the power up defaults.\n");
        printf ("   -- Ok to proceed (y/n)? ");
    } else {
        printf ("WARNING: This application will commit the cameras current settings so that\n");
        printf ("they wil be used as the power up defaults.\n");
        printf ("   -- Ok to proceed (y/n)? ");
    }
    fflush(stdout);

    keyPressed = getAKeystroke ();
    printf("\n");
    if (keyPressed != 'y' && keyPressed != 'Y')
    {
        // User aborted.
        PxLUninitialize (myCamera);
        return A_OK;
    }

    //
    // Step 3.
    //      If requested, load factory defaults
    if (runOption_useFactoryDefaults) {
         rc = PxLLoadSettings (myCamera, FACTORY_DEFAULTS_MEMORY_CHANNEL);
         ASSERT (API_SUCCESS(rc));
    }

    //
    // Step 4.
    //      Save the current settings to the user channel.
    rc = PxLSaveSettings (myCamera, 1); // Channel 0 is reserved for factory defaults
    printf ("  PxLSaveSettings returned 0x%x\n", rc);

    //
    // Step 3.
    //      Done.
    PxLUninitialize (myCamera);
    return 0;
}

int setRunOptions (int argc, char* argv[])
{
    // Loacal versions of each of the run options.
    bool  useFactoryDefaults = runOption_useFactoryDefaults;

    for (int i = 1; i < argc; i++)
    {
        if (!strcmp(argv[i],"-f") ||
            !strcmp(argv[i],"-F")) {
            useFactoryDefaults = true;
        } else {
            return OPPS;
        }
    }

    // command line options are good.  copy our local copies into the static ones used
    // by the app
    runOption_useFactoryDefaults = useFactoryDefaults;

    return A_OK;
}

void usage (char* argv[])
{
    printf ("\n");
	printf ("Set the power on defaults for the connected camera.  Without any parameters, the\n");
	printf ("cameras current parameters will be used.  If the '-f' option is specified\n");
	printf ("then the factory default settings will be used.\n\n");
	printf ("Usage: %s [options]\n", argv[0]);
	printf ("   where options are:\n");
	printf ("     -f  Use factory settings as power on defaults\n");
}



// block until the user presses a key, and then return the key pressed
int getAKeystroke() {

    DontWaitForEnter unbufferedKeyboard;  // Declare this for our getchar
    int   keyPressed;
    bool  done = false;

    // I prefer unbuffered keyboard input, where the user is not required to press enter
    while (!done)
    {
        fflush(stdin);
        if (kbhit())
        {
            done = true;
            keyPressed = getchar();
        }
        usleep (100*1000);  // 100 ms - don't hog all of the CPU
    }

    return keyPressed;
}


