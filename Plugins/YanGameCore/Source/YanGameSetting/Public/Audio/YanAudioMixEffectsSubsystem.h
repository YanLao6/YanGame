#pragma once

#include "Subsystems/WorldSubsystem.h"

#include "YanAudioMixEffectsSubsystem.generated.h"

#define UE_API YANGAMESETTING_API

class USoundControlBus;
class USoundControlBusMix;
class USoundEffectSubmixPreset;
class USoundSubmix;

/** 已解析为硬引用的一条 Submix 效果链。 */
USTRUCT()
struct FYanAudioSubmixEffectsChain
{
	GENERATED_BODY()

	UPROPERTY(Transient)
	TObjectPtr<USoundSubmix> Submix = nullptr;

	// 按数组顺序依次处理
	UPROPERTY(Transient)
	TArray<TObjectPtr<USoundEffectSubmixPreset>> SubmixEffectChain;
};

/**
 * 负责激活默认混音与玩家音量混音，并按玩家的 HDR 音频偏好切换 Submix 效果链。
 * 相关资产在 UYanAudioSettings 中登记，仅在 Game 与 PIE 世界中创建。
 */
UCLASS(MinimalAPI)
class UYanAudioMixEffectsSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	//~Begin USubsystem Interface
	UE_API virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	UE_API virtual void Deinitialize() override;
	UE_API virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	//~End USubsystem Interface

	//~Begin UWorldSubsystem Interface
	UE_API virtual void PostInitialize() override;
	UE_API virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	//~End UWorldSubsystem Interface

	/** 按是否启用 HDR 音频，切换两套 Submix 效果链覆盖 */
	UE_API void ApplyDynamicRangeEffectsChains(bool bHDRAudio);

protected:
	UE_API void OnLoadingScreenStatusChanged(bool bShowingLoadingScreen);
	UE_API void ApplyOrRemoveLoadingScreenMix(bool bWantsLoadingScreenMix);

	//~Begin UWorldSubsystem Interface
	UE_API virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
	//~End UWorldSubsystem Interface

	UPROPERTY(Transient)
	TObjectPtr<USoundControlBusMix> DefaultBaseMix = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<USoundControlBusMix> LoadingScreenMix = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<USoundControlBusMix> UserMix = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<USoundControlBus> OverallControlBus = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<USoundControlBus> MusicControlBus = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<USoundControlBus> SoundFXControlBus = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<USoundControlBus> DialogueControlBus = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<USoundControlBus> VoiceChatControlBus = nullptr;

	UPROPERTY(Transient)
	TArray<FYanAudioSubmixEffectsChain> HDRSubmixEffectChain;

	UPROPERTY(Transient)
	TArray<FYanAudioSubmixEffectsChain> LDRSubmixEffectChain;

	bool bAppliedLoadingScreenMix = false;
};

#undef UE_API
