#pragma once

#include "Engine/DeveloperSettings.h"
#include "UObject/SoftObjectPtr.h"

#include "YanAudioSettings.generated.h"

class USoundEffectSubmixPreset;
class USoundSubmix;

/** 一条 Submix 的效果链覆盖配置。 */
USTRUCT()
struct FYanSubmixEffectChainMap
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, meta = (AllowedClasses = "/Script/Engine.SoundSubmix"))
	TSoftObjectPtr<USoundSubmix> Submix = nullptr;

	// 按数组顺序依次处理
	UPROPERTY(EditAnywhere, meta = (AllowedClasses = "/Script/Engine.SoundEffectSubmixPreset"))
	TArray<TSoftObjectPtr<USoundEffectSubmixPreset>> SubmixEffectChain;
};

/**
 * 音频总线与 Submix 效果链的项目级配置。
 * 这里只登记资产引用，实际激活由 UYanAudioMixEffectsSubsystem 负责。
 */
UCLASS(MinimalAPI, config = Game, defaultconfig, meta = (DisplayName = "Yan Audio Settings"))
class UYanAudioSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	/** 默认基础混音，进入世界即激活 */
	UPROPERTY(config, EditAnywhere, Category = MixSettings, meta = (AllowedClasses = "/Script/AudioModulation.SoundControlBusMix"))
	FSoftObjectPath DefaultControlBusMix;

	/** 加载画面期间叠加的混音，用于压住后台音频 */
	UPROPERTY(config, EditAnywhere, Category = MixSettings, meta = (AllowedClasses = "/Script/AudioModulation.SoundControlBusMix"))
	FSoftObjectPath LoadingScreenControlBusMix;

	/** 承载玩家音量设置的混音 */
	UPROPERTY(config, EditAnywhere, Category = UserMixSettings, meta = (AllowedClasses = "/Script/AudioModulation.SoundControlBusMix"))
	FSoftObjectPath UserSettingsControlBusMix;

	/** 总音量对应的控制总线 */
	UPROPERTY(config, EditAnywhere, Category = UserMixSettings, meta = (AllowedClasses = "/Script/AudioModulation.SoundControlBus"))
	FSoftObjectPath OverallVolumeControlBus;

	/** 音乐音量对应的控制总线 */
	UPROPERTY(config, EditAnywhere, Category = UserMixSettings, meta = (AllowedClasses = "/Script/AudioModulation.SoundControlBus"))
	FSoftObjectPath MusicVolumeControlBus;

	/** 音效音量对应的控制总线 */
	UPROPERTY(config, EditAnywhere, Category = UserMixSettings, meta = (AllowedClasses = "/Script/AudioModulation.SoundControlBus"))
	FSoftObjectPath SoundFXVolumeControlBus;

	/** 对白音量对应的控制总线 */
	UPROPERTY(config, EditAnywhere, Category = UserMixSettings, meta = (AllowedClasses = "/Script/AudioModulation.SoundControlBus"))
	FSoftObjectPath DialogueVolumeControlBus;

	/** 语音聊天音量对应的控制总线 */
	UPROPERTY(config, EditAnywhere, Category = UserMixSettings, meta = (AllowedClasses = "/Script/AudioModulation.SoundControlBus"))
	FSoftObjectPath VoiceChatVolumeControlBus;

	/** 高动态范围输出所用的 Submix 效果链 */
	UPROPERTY(config, EditAnywhere, Category = EffectSettings)
	TArray<FYanSubmixEffectChainMap> HDRAudioSubmixEffectChain;

	/** 低动态范围输出所用的 Submix 效果链 */
	UPROPERTY(config, EditAnywhere, Category = EffectSettings)
	TArray<FYanSubmixEffectChainMap> LDRAudioSubmixEffectChain;
};
