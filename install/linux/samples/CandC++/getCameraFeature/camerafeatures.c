//
// camerafeatures.c
//
// Demonstrates how to get some information about a camera feature.
//
// Note that there are two places to get information about a feature:
// 1) PxLGetCameraFeatures
// 2) PxLGetFeature
//
// PxLGetCameraFeatures can be used to query (generally) static information about 
// a feature. e.g. number of parameters, if it's supported, param max min. etc.
// 
// PxLGetFeature is used to get the feature's current settings/value. 
// (PxLSetFeature is used to set the feature's current settings/value.)
// 
//
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>

#include "camerafeatures.h"

//
// Given a pointer to an individual camera feature, print
// information about the feature
//
//
static void
PrintCameraFeature(CAMERA_FEATURE* pCameraFeature)
{
	U32 i;
	// Is the feature supported?
	int isSupported = ((pCameraFeature->uFlags & FEATURE_FLAG_PRESENCE) == 0) ? 0 : 1;
	if (!isSupported) {
		printf("Feature %d is not supported\n", pCameraFeature->uFeatureId);
	} else {
		printf("Number of parameters: %d\n", pCameraFeature->uNumberOfParameters);
		printf("CAMERA_FEATURE flags:\n");
		DecodeFeatureFlags(pCameraFeature->uFlags);
		for(i=0; i < pCameraFeature->uNumberOfParameters; i++) {
			printf("Parameter %d\n", i);
			printf("  Min value: %f\n", pCameraFeature->pParams[i].fMinValue);
			printf("  Max value: %f\n", pCameraFeature->pParams[i].fMaxValue);
		}
	}
}


//
// Print information about a feature.
// 
// This is one way to determine how many parameters are used by a feature.
// The second way is demonstrated in PrintFeatureTrigger.
// The advantage of this method is that you can also see the max and min values
// a parameters supports.
//
// Note that the max and min are exactly that: max and min.
// It should not be assumed that all values between are supported.
// For example, an ROI width parameter may have a min/max of 0/1600, but 
// widths of 7, 13, 59 etc. are not supported.
//
// Note too that a feature's min and max values may change as other 
// features change. 
// For example, exposure and frame rate are interlinked, and changing
// one may change the min/max for the other.
//
// The feature flags reported by PxLGetCameraFeatures indicate which
// flags are supported (e.g. FEATURE_FLAG_AUTO). They do not indicate
// the current settings; these are available through PxLGetFeature.
//
//
void
PrintFeatureParameterInfo(HANDLE hCamera, U32 featureId)
{
	U32 bufferSize = 0;
	assert(0 != hCamera);

	printf("\n----------------------Feature %d-------------------------\n", featureId);

	// Figure out how much memory we have to allocate
	if (API_SUCCESS(PxLGetCameraFeatures(hCamera, featureId, NULL, &bufferSize))) {
		CAMERA_FEATURES* pFeatureInfo = (CAMERA_FEATURES*)malloc(bufferSize);
		if (NULL != pFeatureInfo) {
			// Now read the information into the buffer
			if (API_SUCCESS(PxLGetCameraFeatures(hCamera, featureId, pFeatureInfo, &bufferSize))) {
				// Do a few sanity checks
				assert(1 == pFeatureInfo->uNumberOfFeatures);
				assert(pFeatureInfo->pFeatures[0].uFeatureId == featureId);

				PrintCameraFeature(&pFeatureInfo->pFeatures[0]);
			}
			free(pFeatureInfo);
		}
	}
	return;
}


//
// In this case, what we'll do is demonstrate the use of FEATURE_ALL to read information
// about all features at once. 
//
// However, we have to be careful because the order of the features is not 
// such that we can just index into the array using the feature id value. 
// Rather, we have to explicitly search the array for the specific feature. 
//
//
void
PrintFeatureParameterInfo2(HANDLE hCamera, U32 featureId)
{
	U32 i;
	U32 bufferSize = 0;
	int featureIndex = -1;

	assert(0 != hCamera);

	printf("\n----------------------Feature %d-------------------------\n", featureId);

	// Figure out how much memory we have to allocate to read info for all the features.
	if (API_SUCCESS(PxLGetCameraFeatures(hCamera, FEATURE_ALL, NULL, &bufferSize))) {
		CAMERA_FEATURES* pAllFeatures = (CAMERA_FEATURES*)malloc(bufferSize);
		if (NULL != pAllFeatures) {
			// Now read the information into the buffer
			if (API_SUCCESS(PxLGetCameraFeatures(hCamera, FEATURE_ALL, pAllFeatures, &bufferSize))) {

				// Do a few sanity checks
				assert(pAllFeatures->uNumberOfFeatures > 1);
				assert(pAllFeatures->uSize == bufferSize);

				// Where in the array of CAMERA_FEATURE structs is the feature we're interested in?
				for(i=0; i < pAllFeatures->uNumberOfFeatures; i++) {
					if (pAllFeatures->pFeatures[i].uFeatureId == featureId) {
						featureIndex = i;
						break;
					}
				}

				// Did we find it?
				if (-1 == featureIndex) {
					printf("ERROR: Unable to find the information for feature %d\n", featureId);
					return;
				}

				PrintCameraFeature(&pAllFeatures->pFeatures[featureIndex]);
			}
			free(pAllFeatures);
		}
	}
	return;
}


//
// FEATURE_SHUTTER is the exposure time.
// 
// We know a priori that FEATURE_EXPOSURE has only one parameter, so can
// just set numParams to 1 and query the value.
//
void
PrintFeatureShutter(HANDLE hCamera)
{
	U32 flags = 0;
	U32 numParams = 1; 
	float exposure;

	assert(0 != hCamera);
	printf("\nPrintFeatureShutter:\n");

	if (API_SUCCESS(PxLGetFeature(hCamera, FEATURE_SHUTTER, &flags, &numParams, &exposure))) {
		printf("Exposure time: %f seconds\n", exposure);
		DecodeFeatureFlags(flags);
	}
}

//
// White Balance
// 
// This is not the RGB white balance, but rather the Color Temperature displayed in Capture OEM.
// For the RGB white balance, see feature FEATURE_WHITE_SHADING.
// 
// Here we assume a colour camera. 
// If you're running this with a mono camera, PxLGetFeature will return an error. 
//
void
PrintFeatureWhiteBalance(HANDLE hCamera)
{
	U32 flags = 0;
	U32 numParams = 1;
	float whiteBalance;

	assert(0 != hCamera);

	printf("\nPrintFeaturewWhiteBalance:\n");

	if (API_SUCCESS(PxLGetFeature(hCamera, FEATURE_WHITE_BAL, &flags, &numParams, &whiteBalance))) {
		printf("Colour Temperature: %f degrees Kelvin\n", whiteBalance);
		DecodeFeatureFlags(flags);
	}
}

//
// This demonstrates one way to determine how many parameters
// a feature requires.
// 
// At this point in time FEATURE_TRIGGER has 5 parameters, so we assert this as
// a sanity check.
//
void
PrintFeatureTrigger(HANDLE hCamera)
{
	U32 flags;
	U32 numParams = 0;
	U32 featureId = FEATURE_TRIGGER;

	assert(0 != hCamera);

	printf("\nPrintFeatureTrigger:\n");

	// Figure out how many params (should be 5)
	if (API_SUCCESS(PxLGetFeature(hCamera, featureId, &flags, &numParams, NULL))) {
		float* pParams = (float*)malloc(sizeof(float) * numParams);
		assert(5 == numParams);
		if(NULL != pParams) {
			if (API_SUCCESS(PxLGetFeature(hCamera, featureId, &flags, &numParams, pParams))) {
				printf("FEATURE_TRIGGER:fMode .... = %d\n", (int)pParams[0]);
				printf("FEATURE_TRIGGER:fType .... = %d (%s)\n", (int)pParams[1], DecodeTriggerType((int)pParams[1]));
				printf("FEATURE_TRIGGER:fPolarity  = %d (%s)\n", (int)pParams[2], DecodePolarity((int)pParams[2]));
				printf("FEATURE_TRIGGER:fDelay ... = %f\n", pParams[3]);
				printf("FEATURE_TRIGGER:fParameter = %f\n", pParams[4]);
				DecodeFeatureFlags(flags);
			}
			free(pParams);
		}
		
	}
}

const char* 
DecodeTriggerType(int triggerType)
{
	switch(triggerType) {
		case TRIGGER_TYPE_FREE_RUNNING: return "TRIGGER_TYPE_FREE_RUNNING";
		case TRIGGER_TYPE_SOFTWARE:		return "TRIGGER_TYPE_SOFTWARE";
		case TRIGGER_TYPE_HARDWARE:		return "TRIGGER_TYPE_HARDWARE";
		default: return "Unkown trigger type";
	}
}

const char* 
DecodePolarity(int polarity)
{
	switch(polarity) {
		case 0: return "negative polarity";
		case 1:	return "positive polarity";
		default: return "Unkown polarity";
	}
	
}

#define NUM_GPIO_PARAMS (6)

//
// At this point in time we assume that GPIO has 6 parameters. 
//
// A more robust approach would check this using the technique
// used in PrintFeatureTrigger().
//
// An error will be reported if you're using a microscopy camera 
// because they don't support GPIO.
//
void
PrintFeatureGPIO(HANDLE hCamera)
{
	U32 flags;
	U32 numParams = NUM_GPIO_PARAMS;
	float params[NUM_GPIO_PARAMS];

	assert(0 != hCamera);

	printf("\nPrintFeatureGPIO:\n");

	memset((void*)&params[0],0,sizeof(params));
	
	// We need to specify which GPIO to use.  We will pick the first GPIO
	params[FEATURE_GPIO_PARAM_GPIO_INDEX] = 1.0;

	if (API_SUCCESS(PxLGetFeature(hCamera, FEATURE_GPIO, &flags, &numParams, &params[0]))) {
		assert(NUM_GPIO_PARAMS == numParams);
		printf("FEATURE_GPIO:fStrobeNumber .... = %d\n", (int)params[0]);
		printf("FEATURE_GPIO:fMode ............ = %d\n", (int)params[1]);
		printf("FEATURE_GPIO:fPolarity......... = %d (%s)\n", (int)params[2], DecodePolarity((int)params[2]));
		printf("FEATURE_GPIO:fParameter1 ...... = %f\n", params[3]);
		printf("FEATURE_GPIO:fParameter2 ...... = %f\n", params[4]);
		printf("FEATURE_GPIO:fParameter3 ...... = %f\n", params[5]);
		DecodeFeatureFlags(flags);
	}
}
	

void 
DecodeFeatureFlags(U32 flags)
{
	if (flags & FEATURE_FLAG_PRESENCE) {
		printf("FEATURE_FLAG_PRESENCE - feature is supported\n");
	}

	if (flags & FEATURE_FLAG_READ_ONLY) {
		printf("FEATURE_FLAG_READ_ONLY - feature can only be read\n");
	}

	if (flags & FEATURE_FLAG_DESC_SUPPORTED) {
		printf("FEATURE_FLAG_DESC_SUPPORTED - feature can be saved to different descriptors\n");
	}

	if (flags & FEATURE_FLAG_MANUAL) {
		printf("FEATURE_FLAG_MANUAL - feature controlled by external app\n");
	}

	if (flags & FEATURE_FLAG_AUTO) {
		printf("FEATURE_FLAG_AUTO - feature automatically controlled by camera\n");
	}

	if (flags & FEATURE_FLAG_ONEPUSH) {
		printf("FEATURE_FLAG_ONEPUSH - camera sets feature only once, then returns to manual operation\n");
	}

	if (flags & FEATURE_FLAG_OFF) {
		printf("FEATURE_FLAG_OFF - feature is set to last known state and cannot be controlled by app\n");
	}

	printf("\n");
	return;
}

//
// Again we assume that this is a color camera. 
// PxLGetFeature will return an error if the camera is a mono camera.
// 
void
PrintFeatureSaturation(HANDLE hCamera)
{
	U32 flags;
	U32 numParams = 1;
	U32 featureId = FEATURE_SATURATION;
	float saturation;

	assert(0 != hCamera);

	printf("\nPrintFeatureSaturation:\n");

	if (API_SUCCESS(PxLGetFeature(hCamera, featureId, &flags, &numParams, &saturation))) {
		assert(1 == numParams);
		printf("\nSaturation: %3.1f%%\n", saturation);
		DecodeFeatureFlags(flags);
	}
}
