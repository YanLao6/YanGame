// Copyright Epic Games, Inc. All Rights Reserved.

#include "Weapon/WeaponStateComponent.h"

#include "Abilities/GameplayAbilityTargetTypes.h"
#include "Equipment/EquipmentManagerComponent.h"
#include "GameFramework/Pawn.h"
#include "GameplayEffectTypes.h"
#include "Kismet/GameplayStatics.h"
#include "ModularTeamSubsystem.h"
#include "NativeGameplayTags.h"
#include "Physics/PhysicalMaterialWithTags.h"
#include "Weapon/RangedWeaponInstance.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(WeaponStateComponent)

UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Gameplay_Zone, "Gameplay.Zone");

UWeaponStateComponent::UWeaponStateComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetIsReplicatedByDefault(true);

	PrimaryComponentTick.bStartWithTickEnabled = true;
	PrimaryComponentTick.bCanEverTick          = true;
}

void UWeaponStateComponent::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// 武器实例不是 Actor 组件，散布度等状态需由持有者代为推进
	if (const APawn* Pawn = GetPawn<APawn>())
	{
		if (UEquipmentManagerComponent* EquipmentManager = Pawn->FindComponentByClass<UEquipmentManagerComponent>())
		{
			if (URangedWeaponInstance* CurrentWeapon = Cast<URangedWeaponInstance>(EquipmentManager->GetFirstInstanceOfType(URangedWeaponInstance::StaticClass())))
			{
				CurrentWeapon->Tick(DeltaTime);
			}
		}
	}
}

bool UWeaponStateComponent::ShouldShowHitAsSuccess(const FHitResult& Hit) const
{
	if (const UModularTeamSubsystem* TeamSubsystem = UWorld::GetSubsystem<UModularTeamSubsystem>(GetWorld()))
	{
		return TeamSubsystem->CanCauseDamage(GetController<APlayerController>(), Hit.GetActor());
	}

	return false;
}

bool UWeaponStateComponent::ShouldUpdateDamageInstigatedTime(const FGameplayEffectContextHandle& EffectContext) const
{
	return EffectContext.GetEffectCauser() != nullptr;
}

void UWeaponStateComponent::ClientConfirmTargetData_Implementation(uint16 UniqueId, bool bSuccess, const TArray<uint8>& HitReplaces)
{
	for (int32 i = 0; i < UnconfirmedServerSideHitMarkers.Num(); i++)
	{
		FServerSideHitMarkerBatch& Batch = UnconfirmedServerSideHitMarkers[i];
		if (Batch.UniqueId != UniqueId)
		{
			continue;
		}

		// HitReplaces 覆盖整批时说明服务器全盘否决，无需生成任何反馈
		if (bSuccess && (HitReplaces.Num() != Batch.Markers.Num()))
		{
			bool  bFoundShowAsSuccessHit = false;
			int32 HitLocationIndex       = 0;

			for (const FScreenSpaceHitLocation& Entry : Batch.Markers)
			{
				if (!HitReplaces.Contains(HitLocationIndex) && Entry.bShowAsSuccess)
				{
					// 整批只需刷新一次时间戳
					if (!bFoundShowAsSuccessHit)
					{
						ActuallyUpdateDamageInstigatedTime();
						bFoundShowAsSuccessHit = true;
					}

					LastWeaponDamageScreenLocations.Add(Entry);
				}

				++HitLocationIndex;
			}
		}

		UnconfirmedServerSideHitMarkers.RemoveAt(i);
		break;
	}
}

void UWeaponStateComponent::AddUnconfirmedServerSideHitMarkers(const FGameplayAbilityTargetDataHandle& InTargetData, const TArray<FHitResult>& FoundHits)
{
	FServerSideHitMarkerBatch& NewUnconfirmedHitMarker = UnconfirmedServerSideHitMarkers.Emplace_GetRef(InTargetData.UniqueId);

	APlayerController* OwnerPC = GetController<APlayerController>();
	if (!OwnerPC)
	{
		return;
	}

	for (const FHitResult& Hit : FoundHits)
	{
		// 投影失败说明命中点在视口之外，不生成标记
		FVector2D HitScreenLocation;
		if (!UGameplayStatics::ProjectWorldToScreen(OwnerPC, Hit.Location, /*out*/ HitScreenLocation, /*bPlayerViewportRelative=*/ false))
		{
			continue;
		}

		FScreenSpaceHitLocation& Entry = NewUnconfirmedHitMarker.Markers.AddDefaulted_GetRef();
		Entry.Location                 = HitScreenLocation;
		Entry.bShowAsSuccess           = ShouldShowHitAsSuccess(Hit);

		// 命中部位取物理材质上首个 Gameplay.Zone 下的标签
		if (const UPhysicalMaterialWithTags* PhysMatWithTags = Cast<const UPhysicalMaterialWithTags>(Hit.PhysMaterial.Get()))
		{
			for (const FGameplayTag MaterialTag : PhysMatWithTags->Tags)
			{
				if (MaterialTag.MatchesTag(TAG_Gameplay_Zone))
				{
					Entry.HitZone = MaterialTag;
					break;
				}
			}
		}
	}
}

void UWeaponStateComponent::UpdateDamageInstigatedTime(const FGameplayEffectContextHandle& EffectContext)
{
	if (ShouldUpdateDamageInstigatedTime(EffectContext))
	{
		ActuallyUpdateDamageInstigatedTime();
	}
}

void UWeaponStateComponent::ActuallyUpdateDamageInstigatedTime()
{
	const UWorld* World = GetWorld();

	// 距上次命中已超过一次连发的间隔，视作新的一轮，清空旧标记
	if (World->GetTimeSeconds() - LastWeaponDamageInstigatedTime > 0.1)
	{
		LastWeaponDamageScreenLocations.Reset();
	}

	LastWeaponDamageInstigatedTime = World->GetTimeSeconds();
}

double UWeaponStateComponent::GetTimeSinceLastHitNotification() const
{
	return GetWorld()->TimeSince(LastWeaponDamageInstigatedTime);
}
