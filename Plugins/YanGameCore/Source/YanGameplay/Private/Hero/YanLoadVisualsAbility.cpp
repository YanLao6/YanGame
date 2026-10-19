#include "Hero/YanLoadVisualsAbility.h"

#include "AbilitySystemComponent.h"
#include "AttributeSets/YanImpulseSet.h"
#include "Components/MeshComponent.h"
#include "GameFramework/Actor.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(YanLoadVisualsAbility)

UYanLoadVisualsAbility::UYanLoadVisualsAbility(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// 纯表现，两端都要跑：本地看自己的负荷，远端看对手的负荷
	InstancingPolicy   = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalOnly;
	ActivationPolicy   = EModularAbilityActivationPolicy::OnSpawn;
}

void UYanLoadVisualsAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	if (ASC == nullptr)
	{
		return;
	}

	CacheMeshComponents();

	LoadChangedDelegateHandle = ASC->GetGameplayAttributeValueChangeDelegate(UYanImpulseSet::GetLoadAttribute())
	                               .AddUObject(this, &UYanLoadVisualsAbility::HandleLoadChanged);

	// 委托只在此后的变化上触发，进场时先按当前值刷一次，避免中途接管的角色停在默认色
	const float MaxLoad = ASC->GetNumericAttribute(UYanImpulseSet::GetMaxLoadAttribute());
	if (MaxLoad > 0.0f)
	{
		PushLoadRatioToMaterials(FMath::Clamp(ASC->GetNumericAttribute(UYanImpulseSet::GetLoadAttribute()) / MaxLoad, 0.0f, 1.0f));
	}
}

void UYanLoadVisualsAbility::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	if (LoadChangedDelegateHandle.IsValid())
	{
		if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
		{
			ASC->GetGameplayAttributeValueChangeDelegate(UYanImpulseSet::GetLoadAttribute()).Remove(LoadChangedDelegateHandle);
		}

		LoadChangedDelegateHandle.Reset();
	}

	CachedMeshes.Reset();

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UYanLoadVisualsAbility::HandleLoadChanged(const FOnAttributeChangeData& ChangeData)
{
	const UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	if (ASC == nullptr)
	{
		return;
	}

	const float MaxLoad = ASC->GetNumericAttribute(UYanImpulseSet::GetMaxLoadAttribute());
	if (MaxLoad <= 0.0f)
	{
		return;
	}

	PushLoadRatioToMaterials(FMath::Clamp(ChangeData.NewValue / MaxLoad, 0.0f, 1.0f));
}

void UYanLoadVisualsAbility::PushLoadRatioToMaterials(float LoadRatio) const
{
	for (UMeshComponent* Mesh : CachedMeshes)
	{
		if (Mesh == nullptr)
		{
			continue;
		}

		Mesh->SetScalarParameterValueOnMaterials(LoadRatioParameterName, LoadRatio);
	}
}

void UYanLoadVisualsAbility::CacheMeshComponents()
{
	CachedMeshes.Reset();

	if (const AActor* Avatar = GetAvatarActorFromActorInfo())
	{
		Avatar->GetComponents<UMeshComponent>(CachedMeshes);
	}
}
