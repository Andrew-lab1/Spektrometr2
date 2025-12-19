/*
// A simple demonstration of reading the XML file for a given camera 
 */
#include "PixeLINKApi.h"

#include <unistd.h>
#include <stdio.h>
#include <string>

#define EXIT_SUCCESS    0
#define EXIT_FAILURE    1

using namespace std;

int 
main(int argc, char* argv[])
{
	// Initialize any camera. 
	HANDLE hCamera;
	PXL_RETURN_CODE rc = PxLInitialize(0, &hCamera);
	if (!API_SUCCESS(rc)) {
		printf("ERROR: Unable to initialize a camera (0x%8.8X)\n", rc);
		return EXIT_FAILURE;
	}

   // Grab the XML from the camera
   string xmlFile;
   U32    xmlFileSize = 0;

   // Determine the size of the XML file
   rc = PxLGetCameraXML (hCamera, NULL, &xmlFileSize);
	if (!API_SUCCESS(rc)) {
	   printf("ERROR: Cannot read the XML file this camera(0x%8.8X)\n", rc);
      PxLUninitialize(hCamera);
	   return EXIT_FAILURE;
	 }
    xmlFile.resize (xmlFileSize);
    // Now 'get' the actual XML file
    rc = PxLGetCameraXML (hCamera, (LPVOID)xmlFile.c_str(), &xmlFileSize);
	 if (!API_SUCCESS(rc)) {
		printf("ERROR: Cannot read the XML file this camera (0x%8.8X)\n", rc);
    	PxLUninitialize(hCamera);
		return EXIT_FAILURE;
	 }
    printf ("=================== Camera's XML file (%d bytes) =====================\n", xmlFileSize);
    printf ("%s", xmlFile.c_str());

	 PxLUninitialize(hCamera);
	 return EXIT_SUCCESS;

}



