//
// camerafeatures.h
//

#ifndef CAMERAFEATURES_H
#define CAMERAFEATURES_H

#include <PixeLINKApi.h>

void PrintFeatureParameterInfo	(HANDLE hCamera, U32 featureId);
void PrintFeatureParameterInfo2	(HANDLE hCamera, U32 featureId);

void PrintFeatureShutter		(HANDLE hCamera);
void PrintFeatureWhiteBalance	(HANDLE hCamera);
void PrintFeatureTrigger		(HANDLE hCamera);
void PrintFeatureSaturation		(HANDLE hCamera);
void PrintFeatureGPIO			(HANDLE hCamera);

const char* DecodeTriggerType(int triggerType);
const char* DecodePolarity(int polarity);

void DecodeFeatureFlags(U32 flags);

#endif
