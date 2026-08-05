#include "Kinesis/YanKinesisTargetComponent.h"

#include "Kinesis/YanKinesisRegistrySubsystem.h"
#include "Components/SceneComponent.h"
#include "GameFramework/Actor.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(YanKinesisTargetComponent)

UYanKinesisTargetComponent::UYanKinesisTargetComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UYanKinesisTargetComponent::BeginPlay()
{
	Super::BeginPlay();

	if (UYanKinesisRegistrySubsystem* Registry = UYanKinesisRegistrySubsystem::Get(this))
	{
		Registry->RegisterTarget(this);
	}
}

void UYanKinesisTargetComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UYanKinesisRegistrySubsystem* Registry = UYanKinesisRegistrySubsystem::Get(this))
	{
		Registry->UnregisterTarget(this);
	}

	Super::EndPlay(EndPlayReason);
}

UObject* UYanKinesisTargetComponent::ResolveInterfaceProvider() const
{
	AActor* Owner = GetOwner();

	// Owner 的实现表达的是该 Actor 自身的规则，比本组件的通用默认更贴近意图
	if (Owner && Owner->Implements<UYanKinesisControllable>())
	{
		return Owner;
	}

	return const_cast<UYanKinesisTargetComponent*>(this);
}

bool UYanKinesisTargetComponent::IsControllableBy(const AActor* InInstigator) const
{
	const AActor* Owner = GetOwner();
	if (!IsValid(Owner) || Owner == InInstigator)
	{
		return false;
	}

	return IYanKinesisControllable::Execute_CanBeKinesisControlled(ResolveInterfaceProvider(), InInstigator);
}

USceneComponent* UYanKinesisTargetComponent::ResolveAnchorComponent() const
{
	if (USceneComponent* Anchor = IYanKinesisControllable::Execute_GetKinesisAnchorComponent(ResolveInterfaceProvider()))
	{
		return Anchor;
	}

	AActor* Owner = GetOwner();

	return IsValid(Owner) ? Owner->GetRootComponent() : nullptr;
}

void UYanKinesisTargetComponent::NotifyControlBegin(AActor* InInstigator)
{
	IYanKinesisControllable::Execute_OnKinesisControlBegin(ResolveInterfaceProvider(), InInstigator);
}

void UYanKinesisTargetComponent::NotifyControlEnd(AActor* InInstigator)
{
	IYanKinesisControllable::Execute_OnKinesisControlEnd(ResolveInterfaceProvider(), InInstigator);
}

bool UYanKinesisTargetComponent::CanBeKinesisControlled_Implementation(const AActor* InInstigator) const
{
	return bKinesisEnabled;
}

USceneComponent* UYanKinesisTargetComponent::GetKinesisAnchorComponent_Implementation() const
{
	// FComponentReference 只在同一 Actor 内解析，配错目标时返回空并由调用方回落根组件
	return Cast<USceneComponent>(AnchorComponent.GetComponent(GetOwner()));
}
