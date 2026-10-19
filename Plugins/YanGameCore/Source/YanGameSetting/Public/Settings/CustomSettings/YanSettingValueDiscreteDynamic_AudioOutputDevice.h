#pragma once

#include "AudioMixerBlueprintLibrary.h"
#include "GameSettingValueDiscreteDynamic.h"

#include "YanSettingValueDiscreteDynamic_AudioOutputDevice.generated.h"

#define UE_API YANGAMESETTING_API

enum class EAudioDeviceChangedRole : uint8;

/**
 * 音频输出设备选择项。
 * 选项异步枚举得到，并随设备增删与系统默认设备变更刷新。
 */
UCLASS(MinimalAPI)
class UYanSettingValueDiscreteDynamic_AudioOutputDevice : public UGameSettingValueDiscreteDynamic
{
	GENERATED_BODY()

public:
	virtual ~UYanSettingValueDiscreteDynamic_AudioOutputDevice() = default;

	//~Begin UGameSettingValueDiscrete Interface
	UE_API virtual void SetDiscreteOptionByIndex(int32 Index) override;
	//~End UGameSettingValueDiscrete Interface

	UFUNCTION()
	UE_API void OnAudioOutputDevicesObtained(const TArray<FAudioOutputDeviceInfo>& AvailableDevices);

	UFUNCTION()
	UE_API void OnCompletedDeviceSwap(const FSwapAudioOutputResult& SwapResult);

	UFUNCTION()
	UE_API void DeviceAddedOrRemoved(FString DeviceId);

	UFUNCTION()
	UE_API void DefaultDeviceChanged(EAudioDeviceChangedRole InRole, FString DeviceId);

protected:
	//~Begin UGameSettingValue Interface
	UE_API virtual void OnInitialized() override;
	//~End UGameSettingValue Interface

	TArray<FAudioOutputDeviceInfo> OutputDevices;
	FString                        CurrentDeviceId;
	FString                        SystemDefaultDeviceId;
	int32                          LastKnownGoodIndex = 0;
	bool                           bRequestDefault    = false;

	FOnAudioOutputDevicesObtained DevicesObtainedCallback;
	FOnCompletedDeviceSwap        DevicesSwappedCallback;
};

#undef UE_API
