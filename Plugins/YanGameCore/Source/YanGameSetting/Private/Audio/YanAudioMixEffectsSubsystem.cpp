#include "Audio/YanAudioMixEffectsSubsystem.h"

#include "Audio/YanAudioSettings.h"
#include "AudioMixerBlueprintLibrary.h"
#include "AudioModulationStatics.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "LoadingScreenManager.h"
#include "Settings/YanSettingsLocal.h"
#include "Sound/SoundEffectSubmix.h"
#include "SoundControlBus.h"
#include "SoundControlBusMix.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(YanAudioMixEffectsSubsystem)

namespace
{
	/** 把配置中的软引用效果链解析为硬引用。 */
	void ResolveSubmixEffectChains(const TArray<FYanSubmixEffectChainMap>& SoftChains, TArray<FYanAudioSubmixEffectsChain>& OutChains)
	{
		for (const FYanSubmixEffectChainMap& SoftSubmixEffectChain : SoftChains)
		{
			FYanAudioSubmixEffectsChain NewEffectChain;

			if (USoundSubmix* Submix = Cast<USoundSubmix>(SoftSubmixEffectChain.Submix.LoadSynchronous()))
			{
				NewEffectChain.Submix = Submix;

				for (const TSoftObjectPtr<USoundEffectSubmixPreset>& SoftEffect : SoftSubmixEffectChain.SubmixEffectChain)
				{
					if (USoundEffectSubmixPreset* SubmixPreset = Cast<USoundEffectSubmixPreset>(SoftEffect.LoadSynchronous()))
					{
						NewEffectChain.SubmixEffectChain.Add(SubmixPreset);
					}
				}
			}

			OutChains.Add(NewEffectChain);
		}
	}
}

void UYanAudioMixEffectsSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
}

void UYanAudioMixEffectsSubsystem::Deinitialize()
{
	if (ULoadingScreenManager* LoadingScreenManager = UGameInstance::GetSubsystem<ULoadingScreenManager>(GetWorld()->GetGameInstance()))
	{
		LoadingScreenManager->OnLoadingScreenVisibilityChangedDelegate().RemoveAll(this);
		ApplyOrRemoveLoadingScreenMix(false);
	}

	Super::Deinitialize();
}

bool UYanAudioMixEffectsSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	bool bShouldCreateSubsystem = Super::ShouldCreateSubsystem(Outer);

	if (Outer)
	{
		if (const UWorld* World = Outer->GetWorld())
		{
			bShouldCreateSubsystem = DoesSupportWorldType(World->WorldType) && bShouldCreateSubsystem;
		}
	}

	return bShouldCreateSubsystem;
}

void UYanAudioMixEffectsSubsystem::PostInitialize()
{
	Super::PostInitialize();

	if (const UYanAudioSettings* AudioSettings = GetDefault<UYanAudioSettings>())
	{
		DefaultBaseMix   = Cast<USoundControlBusMix>(AudioSettings->DefaultControlBusMix.TryLoad());
		LoadingScreenMix = Cast<USoundControlBusMix>(AudioSettings->LoadingScreenControlBusMix.TryLoad());
		UserMix          = Cast<USoundControlBusMix>(AudioSettings->UserSettingsControlBusMix.TryLoad());

		OverallControlBus   = Cast<USoundControlBus>(AudioSettings->OverallVolumeControlBus.TryLoad());
		MusicControlBus     = Cast<USoundControlBus>(AudioSettings->MusicVolumeControlBus.TryLoad());
		SoundFXControlBus   = Cast<USoundControlBus>(AudioSettings->SoundFXVolumeControlBus.TryLoad());
		DialogueControlBus  = Cast<USoundControlBus>(AudioSettings->DialogueVolumeControlBus.TryLoad());
		VoiceChatControlBus = Cast<USoundControlBus>(AudioSettings->VoiceChatVolumeControlBus.TryLoad());

		ResolveSubmixEffectChains(AudioSettings->HDRAudioSubmixEffectChain, HDRSubmixEffectChain);
		ResolveSubmixEffectChains(AudioSettings->LDRAudioSubmixEffectChain, LDRSubmixEffectChain);
	}

	if (ULoadingScreenManager* LoadingScreenManager = UGameInstance::GetSubsystem<ULoadingScreenManager>(GetWorld()->GetGameInstance()))
	{
		LoadingScreenManager->OnLoadingScreenVisibilityChangedDelegate().AddUObject(this, &ThisClass::OnLoadingScreenStatusChanged);
		ApplyOrRemoveLoadingScreenMix(LoadingScreenManager->GetLoadingScreenDisplayStatus());
	}
}

void UYanAudioMixEffectsSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);

	const UWorld* World = InWorld.GetWorld();
	if (!World)
	{
		return;
	}

	if (DefaultBaseMix)
	{
		UAudioModulationStatics::ActivateBusMix(World, DefaultBaseMix);
	}

	const UYanSettingsLocal* SettingsLocal = GetDefault<UYanSettingsLocal>();
	if (!SettingsLocal)
	{
		return;
	}

	if (UserMix)
	{
		UAudioModulationStatics::ActivateBusMix(World, UserMix);

		// 五条总线缺一不可，否则混音级次不完整，索性整体跳过
		if (OverallControlBus && MusicControlBus && SoundFXControlBus && DialogueControlBus && VoiceChatControlBus)
		{
			const TArray<FSoundControlBusMixStage> ControlBusMixStageArray = {
				UAudioModulationStatics::CreateBusMixStage(World, OverallControlBus, SettingsLocal->GetOverallVolume()),
				UAudioModulationStatics::CreateBusMixStage(World, MusicControlBus, SettingsLocal->GetMusicVolume()),
				UAudioModulationStatics::CreateBusMixStage(World, SoundFXControlBus, SettingsLocal->GetSoundFXVolume()),
				UAudioModulationStatics::CreateBusMixStage(World, DialogueControlBus, SettingsLocal->GetDialogueVolume()),
				UAudioModulationStatics::CreateBusMixStage(World, VoiceChatControlBus, SettingsLocal->GetVoiceChatVolume())
			};

			UAudioModulationStatics::UpdateMix(World, UserMix, ControlBusMixStageArray);
		}
	}

	ApplyDynamicRangeEffectsChains(SettingsLocal->IsHDRAudioModeEnabled());
}

void UYanAudioMixEffectsSubsystem::ApplyDynamicRangeEffectsChains(bool bHDRAudio)
{
	const TArray<FYanAudioSubmixEffectsChain>& ChainToApply = bHDRAudio ? HDRSubmixEffectChain : LDRSubmixEffectChain;
	const TArray<FYanAudioSubmixEffectsChain>& ChainToClear = bHDRAudio ? LDRSubmixEffectChain : HDRSubmixEffectChain;

	// 只清除不会被新链覆盖的 Submix，其余交给覆盖操作即可
	TArray<USoundSubmix*> SubmixesLeftToClear;
	for (const FYanAudioSubmixEffectsChain& EffectChainToClear : ChainToClear)
	{
		const bool bWillBeOverridden = ChainToApply.ContainsByPredicate(
			[&EffectChainToClear](const FYanAudioSubmixEffectsChain& Chain) { return Chain.Submix == EffectChainToClear.Submix; });

		if (!bWillBeOverridden)
		{
			SubmixesLeftToClear.Add(EffectChainToClear.Submix);
		}
	}

	for (const FYanAudioSubmixEffectsChain& SubmixEffectChain : ChainToApply)
	{
		if (SubmixEffectChain.Submix)
		{
			UAudioMixerBlueprintLibrary::SetSubmixEffectChainOverride(GetWorld(), SubmixEffectChain.Submix, SubmixEffectChain.SubmixEffectChain, 0.1f);
		}
	}

	for (USoundSubmix* Submix : SubmixesLeftToClear)
	{
		UAudioMixerBlueprintLibrary::ClearSubmixEffectChainOverride(GetWorld(), Submix, 0.1f);
	}
}

void UYanAudioMixEffectsSubsystem::OnLoadingScreenStatusChanged(bool bShowingLoadingScreen)
{
	ApplyOrRemoveLoadingScreenMix(bShowingLoadingScreen);
}

void UYanAudioMixEffectsSubsystem::ApplyOrRemoveLoadingScreenMix(bool bWantsLoadingScreenMix)
{
	UWorld* World = GetWorld();

	if ((bAppliedLoadingScreenMix == bWantsLoadingScreenMix) || !LoadingScreenMix || !World)
	{
		return;
	}

	if (bWantsLoadingScreenMix)
	{
		UAudioModulationStatics::ActivateBusMix(World, LoadingScreenMix);
	}
	else
	{
		UAudioModulationStatics::DeactivateBusMix(World, LoadingScreenMix);
	}

	bAppliedLoadingScreenMix = bWantsLoadingScreenMix;
}

bool UYanAudioMixEffectsSubsystem::DoesSupportWorldType(const EWorldType::Type World) const
{
	// 只有实际运行游戏的世界需要混音，编辑器预览等一律跳过
	return (World == EWorldType::Game || World == EWorldType::PIE);
}
