#include "AnimNotify/AnimNotify_YanContextEffects.h"

#include "Components/SkeletalMeshComponent.h"
#include "ContextEffects/YanContextEffectsLibrary.h"
#include "Engine/World.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AnimNotify_YanContextEffects)

FString UAnimNotify_YanContextEffects::GetNotifyName_Implementation() const
{
	if (Effect.IsValid())
	{
		return Effect.ToString();
	}

	return Super::GetNotifyName_Implementation();
}

void UAnimNotify_YanContextEffects::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	AActor* OwningActor = MeshComp ? MeshComp->GetOwner() : nullptr;
	UWorld* World = OwningActor ? OwningActor->GetWorld() : nullptr;
	if (!World)
	{
		return;
	}

	// 专用服务器无表现层需求，提前退出以省掉射线检测
	const bool bIsEditorPreview = World->WorldType == EWorldType::EditorPreview;
	if (!bIsEditorPreview && World->GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	FYanContextEffectRequest Request;
	Request.EffectTag = Effect;
	Request.AttachToComponent = MeshComp;
	Request.AttachPoint = bAttached ? SocketName : NAME_None;
	Request.LocationOffset = LocationOffset;
	Request.RotationOffset = RotationOffset;
	Request.VFXScale = VFXScale;
	Request.AudioVolume = AudioProperties.VolumeMultiplier;
	Request.AudioPitch = AudioProperties.PitchMultiplier;

	if (bPerformTrace)
	{
		FCollisionQueryParams QueryParams;
		QueryParams.bReturnPhysicalMaterial = true;
		if (TraceProperties.bIgnoreActor)
		{
			QueryParams.AddIgnoredActor(OwningActor);
		}

		const FVector TraceStart = bAttached ? MeshComp->GetSocketLocation(SocketName) : MeshComp->GetComponentLocation();

		FHitResult HitResult;
		if (World->LineTraceSingleByChannel(HitResult, TraceStart, TraceStart + TraceProperties.EndTraceLocationOffset,
			TraceProperties.TraceChannel, QueryParams, FCollisionResponseParams::DefaultResponseParam))
		{
			UYanContextEffectsFunctionLibrary::AppendSurfaceContexts(HitResult.PhysMaterial.Get(), Request.Contexts);
		}
	}

	UYanContextEffectsFunctionLibrary::DispatchContextEffect(OwningActor, Request);

#if WITH_EDITOR
	if (bPreviewInEditor && bIsEditorPreview)
	{
		PreviewInEditor(Request);
	}
#endif
}

#if WITH_EDITOR
void UAnimNotify_YanContextEffects::PreviewInEditor(const FYanContextEffectRequest& Request) const
{
	UYanContextEffectsLibrary* PreviewLibrary = Cast<UYanContextEffectsLibrary>(PreviewProperties.PreviewContextEffectsLibrary.TryLoad());
	if (!PreviewLibrary)
	{
		return;
	}

	// 预览世界没有响应方组件，库只能来自预览属性；且不能容忍首次播放静默，因此阻塞加载
	PreviewLibrary->LoadSynchronousForPreview();

	FGameplayTagContainer Contexts = Request.Contexts;
	Contexts.AppendTags(PreviewProperties.PreviewContexts);

	const TObjectPtr<UYanContextEffectsLibrary> PreviewLibraries[] = { PreviewLibrary };
	UYanContextEffectsFunctionLibrary::PlayEffectsFromLibraries(Request, Contexts, PreviewLibraries);
}
#endif
