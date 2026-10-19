#include "Input/YanAimSensitivityData.h"

#include "Settings/YanSettingsShared.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(YanAimSensitivityData)

UYanAimSensitivityData::UYanAimSensitivityData(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SensitivityMap = {
		{EYanGamepadSensitivity::Slow, 0.5f},
		{EYanGamepadSensitivity::SlowPlus, 0.75f},
		{EYanGamepadSensitivity::SlowPlusPlus, 0.9f},
		{EYanGamepadSensitivity::Normal, 1.0f},
		{EYanGamepadSensitivity::NormalPlus, 1.1f},
		{EYanGamepadSensitivity::NormalPlusPlus, 1.25f},
		{EYanGamepadSensitivity::Fast, 1.5f},
		{EYanGamepadSensitivity::FastPlus, 1.75f},
		{EYanGamepadSensitivity::FastPlusPlus, 2.0f},
		{EYanGamepadSensitivity::Insane, 2.5f},
	};
}

float UYanAimSensitivityData::SensitivityEnumToFloat(const EYanGamepadSensitivity InSensitivity) const
{
	if (const float* Sensitivity = SensitivityMap.Find(InSensitivity))
	{
		return *Sensitivity;
	}

	return 1.0f;
}
