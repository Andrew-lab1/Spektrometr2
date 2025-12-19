//
// main.c
//
// Demonstrates how to get some camera feature information.
//

#include "camerafeatures.h"

int 
main(int argc, char* argv[])
{
	HANDLE hCamera;

	// We assume there's only one camera connected;
	if (API_SUCCESS(PxLInitialize(0, &hCamera))) {

		// Print some information about the camera
		PrintFeatureParameterInfo(hCamera, FEATURE_SHUTTER);
		PrintFeatureShutter(hCamera);

		PrintFeatureParameterInfo(hCamera, FEATURE_WHITE_BAL);
		PrintFeatureWhiteBalance(hCamera);

		PrintFeatureParameterInfo(hCamera, FEATURE_TRIGGER);
		PrintFeatureTrigger(hCamera);

		PrintFeatureParameterInfo(hCamera, FEATURE_GPIO);
		PrintFeatureGPIO(hCamera);

		PrintFeatureParameterInfo(hCamera, FEATURE_SATURATION);
		PrintFeatureSaturation(hCamera);

		// Demonstrate two ways to get the same information
		PrintFeatureParameterInfo (hCamera, FEATURE_ROI);
		PrintFeatureParameterInfo2(hCamera, FEATURE_ROI);

		// Uninitialize the camera now that we're done with it.
		PxLUninitialize(hCamera);
	}
	return 0;
}

