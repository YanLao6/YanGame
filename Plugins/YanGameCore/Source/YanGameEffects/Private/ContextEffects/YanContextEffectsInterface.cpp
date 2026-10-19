#include "ContextEffects/YanContextEffectsInterface.h"

#include "Components/AudioComponent.h"
#include "ContextEffects/YanContextEffectsLibrary.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Physics/PhysicalMaterialWithTags.h"
#include "Sound/SoundBase.h"
#include "YanGameEffects.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(YanContextEffectsInterface)

void UYanContextEffectsFunctionLibrary::DispatchContextEffect(AActor* Target, const FYanContextEffectRequest& Request)
{
	if (!Target)
	{
		return;
	}

	const UWorld* World = Target->GetWorld();
	if (!World || World->GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	if (Target->Implements<UYanContextEffectsInterface>())
	{
		IYanContextEffectsInterface::Execute_PlayContextEffect(Target, Request);
	}

	// 逐个组件判定而非 GetComponentsByInterface：后者每次事件都要分配一个临时数组，
	// 而本函数处于脚步级频率的热路径上
	for (UActorComponent* Component : Target->GetComponents())
	{
		if (Component && Component->Implements<UYanContextEffectsInterface>())
		{
			IYanContextEffectsInterface::Execute_PlayContextEffect(Component, Request);
		}
	}
}

void UYanContextEffectsFunctionLibrary::AppendSurfaceContexts(const UPhysicalMaterial* PhysicalMaterial, FGameplayTagContainer& OutContexts)
{
	if (const UPhysicalMaterialWithTags* TaggedMaterial = Cast<const UPhysicalMaterialWithTags>(PhysicalMaterial))
	{
		OutContexts.AppendTags(TaggedMaterial->Tags);
	}
}

void UYanContextEffectsFunctionLibrary::PlayEffectsFromLibraries(const FYanContextEffectRequest& Request,
	const FGameplayTagContainer& ResolvedContexts, TArrayView<const TObjectPtr<UYanContextEffectsLibrary>> Libraries)
{
	if (!Request.AttachToComponent || !Request.EffectTag.IsValid())
	{
		return;
	}

	TArray<USoundBase*> Sounds;
	TArray<UNiagaraSystem*> NiagaraSystems;

	for (const TObjectPtr<UYanContextEffectsLibrary>& Library : Libraries)
	{
		if (!Library)
		{
			continue;
		}

		if (!Library->IsLoaded())
		{
			UE_LOG(LogYanContextEffects, Verbose, TEXT("特效库 %s 尚未加载完成，丢弃事件 %s"),
				*Library->GetName(), *Request.EffectTag.ToString());
			continue;
		}

		Library->GatherEffects(Request.EffectTag, ResolvedContexts, Sounds, NiagaraSystems);
	}

	for (USoundBase* Sound : Sounds)
	{
		UGameplayStatics::SpawnSoundAttached(Sound, Request.AttachToComponent, Request.AttachPoint,
			Request.LocationOffset, Request.RotationOffset, EAttachLocation::KeepRelativeOffset,
			false, Request.AudioVolume, Request.AudioPitch, 0.0f, nullptr, nullptr, true);
	}

	for (UNiagaraSystem* NiagaraSystem : NiagaraSystems)
	{
		UNiagaraFunctionLibrary::SpawnSystemAttached(NiagaraSystem, Request.AttachToComponent, Request.AttachPoint,
			Request.LocationOffset, Request.RotationOffset, Request.VFXScale, EAttachLocation::KeepRelativeOffset,
			true, ENCPoolMethod::None, true, true);
	}
}
