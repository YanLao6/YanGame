#include "Settings/CustomSettings/YanSettingValueDiscreteDynamic_AudioOutputDevice.h"

#include "AudioDeviceNotificationSubsystem.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(YanSettingValueDiscreteDynamic_AudioOutputDevice)

#define LOCTEXT_NAMESPACE "YanSettings"

void UYanSettingValueDiscreteDynamic_AudioOutputDevice::OnInitialized()
{
	Super::OnInitialized();

	DevicesObtainedCallback.BindUFunction(this, FName("OnAudioOutputDevicesObtained"));
	DevicesSwappedCallback.BindUFunction(this, FName("OnCompletedDeviceSwap"));

	if (UAudioDeviceNotificationSubsystem* AudioDeviceNotifSubsystem = UAudioDeviceNotificationSubsystem::Get())
	{
		AudioDeviceNotifSubsystem->DeviceAddedNative.AddUObject(this, &UYanSettingValueDiscreteDynamic_AudioOutputDevice::DeviceAddedOrRemoved);
		AudioDeviceNotifSubsystem->DeviceRemovedNative.AddUObject(this, &UYanSettingValueDiscreteDynamic_AudioOutputDevice::DeviceAddedOrRemoved);
		AudioDeviceNotifSubsystem->DefaultRenderDeviceChangedNative.AddUObject(this, &UYanSettingValueDiscreteDynamic_AudioOutputDevice::DefaultDeviceChanged);
	}

	UAudioMixerBlueprintLibrary::GetAvailableAudioOutputDevices(this, DevicesObtainedCallback);
}

void UYanSettingValueDiscreteDynamic_AudioOutputDevice::OnAudioOutputDevicesObtained(const TArray<FAudioOutputDeviceInfo>& AvailableDevices)
{
	int32 NewSize = AvailableDevices.Num();
	OutputDevices.Reset(NewSize++);
	OutputDevices.Append(AvailableDevices);

	OptionValues.Reset(NewSize);
	OptionDisplayTexts.Reset(NewSize);

	// 占位项必须排在最前，稍后才能把系统默认设备名填进它的显示文本
	AddDynamicOption(TEXT(""), FText::GetEmpty());
	FString SystemDefaultDeviceName;

	for (const FAudioOutputDeviceInfo& DeviceInfo : OutputDevices)
	{
		if (DeviceInfo.DeviceId.IsEmpty() || DeviceInfo.Name.IsEmpty())
		{
			continue;
		}

		if (DeviceInfo.bIsSystemDefault)
		{
			SystemDefaultDeviceId   = DeviceInfo.DeviceId;
			SystemDefaultDeviceName = DeviceInfo.Name;
		}

		if (DeviceInfo.bIsCurrentDevice)
		{
			CurrentDeviceId = DeviceInfo.DeviceId;
		}

		AddDynamicOption(DeviceInfo.DeviceId, FText::FromString(DeviceInfo.Name));
	}

	OptionDisplayTexts[0] = FText::Format(LOCTEXT("DefaultAudioOutputDevice", "默认输出 - {0}"), FText::FromString(SystemDefaultDeviceName));

	SetDefaultValueFromString(TEXT(""));
	RefreshEditableState();
}

void UYanSettingValueDiscreteDynamic_AudioOutputDevice::OnCompletedDeviceSwap(const FSwapAudioOutputResult& SwapResult)
{
	// 设备切换的失败回滚尚未启用，见 SetDiscreteOptionByIndex 的说明
}

void UYanSettingValueDiscreteDynamic_AudioOutputDevice::DeviceAddedOrRemoved(FString DeviceId)
{
	UAudioMixerBlueprintLibrary::GetAvailableAudioOutputDevices(this, DevicesObtainedCallback);
}

void UYanSettingValueDiscreteDynamic_AudioOutputDevice::DefaultDeviceChanged(EAudioDeviceChangedRole InRole, FString DeviceId)
{
	UAudioMixerBlueprintLibrary::GetAvailableAudioOutputDevices(this, DevicesObtainedCallback);
}

void UYanSettingValueDiscreteDynamic_AudioOutputDevice::SetDiscreteOptionByIndex(int32 Index)
{
	// 仅记录选择，不主动调用 SwapAudioOutputDevice：
	// 引擎侧的设备切换在部分声卡上会失败且难以可靠回滚，故沿用系统默认输出
	Super::SetDiscreteOptionByIndex(Index);
}

#undef LOCTEXT_NAMESPACE
