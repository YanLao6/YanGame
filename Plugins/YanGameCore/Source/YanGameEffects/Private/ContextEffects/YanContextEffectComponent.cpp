#include "ContextEffects/YanContextEffectComponent.h"

#include "Components/SkeletalMeshComponent.h"
#include "ContextEffects/YanContextEffectsLibrary.h"
#include "GameFramework/Actor.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(YanContextEffectComponent)

UYanContextEffectComponent::UYanContextEffectComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	bAutoActivate = true;
}

void UYanContextEffectComponent::BeginPlay()
{
	Super::BeginPlay();

	CurrentContexts = DefaultEffectContexts;
	UpdateLibraries(DefaultContextEffectsLibraries);
}

void UYanContextEffectComponent::PlayContextEffect_Implementation(const FYanContextEffectRequest& Request)
{
	if (ResolvedLibraries.IsEmpty())
	{
		return;
	}

	FGameplayTagContainer ResolvedContexts = Request.Contexts;
	ResolvedContexts.AppendTags(CurrentContexts);

	if (Request.AttachToComponent)
	{
		UYanContextEffectsFunctionLibrary::PlayEffectsFromLibraries(Request, ResolvedContexts, ResolvedLibraries);
		return;
	}

	FYanContextEffectRequest ResolvedRequest = Request;
	ResolvedRequest.AttachToComponent = ResolveAttachTarget();
	UYanContextEffectsFunctionLibrary::PlayEffectsFromLibraries(ResolvedRequest, ResolvedContexts, ResolvedLibraries);
}

void UYanContextEffectComponent::UpdateEffectContexts(const FGameplayTagContainer& NewEffectContexts)
{
	CurrentContexts = NewEffectContexts;
}

void UYanContextEffectComponent::UpdateLibraries(const TSet<TSoftObjectPtr<UYanContextEffectsLibrary>>& NewContextEffectsLibraries)
{
	ResolvedLibraries.Reset();

	// 上下文特效是纯表现层，专用服务器上不解析也就不会加载任何音效与 Niagara 资源
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	ResolvedLibraries.Reserve(NewContextEffectsLibraries.Num());
	for (const TSoftObjectPtr<UYanContextEffectsLibrary>& SoftLibrary : NewContextEffectsLibraries)
	{
		if (UYanContextEffectsLibrary* Library = SoftLibrary.LoadSynchronous())
		{
			Library->RequestLoad();
			ResolvedLibraries.Add(Library);
		}
	}
}

USceneComponent* UYanContextEffectComponent::ResolveAttachTarget() const
{
	const AActor* Owner = GetOwner();
	if (!Owner)
	{
		return nullptr;
	}

	if (USceneComponent* Mesh = Owner->FindComponentByClass<USkeletalMeshComponent>())
	{
		return Mesh;
	}

	return Owner->GetRootComponent();
}
