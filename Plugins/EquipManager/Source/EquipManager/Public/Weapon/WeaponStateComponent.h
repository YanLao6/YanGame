// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/ControllerComponent.h"
#include "GameplayTagContainer.h"

#include "WeaponStateComponent.generated.h"

#define UE_API EQUIPMANAGER_API

struct FGameplayAbilityTargetDataHandle;
struct FGameplayEffectContextHandle;
struct FHitResult;

/**
 * 一次命中在准星上的屏幕空间标记。
 * bShowAsSuccess 区分「打到了东西」与「打到了可造成伤害的目标」。
 */
struct FScreenSpaceHitLocation
{
	// 视口屏幕空间坐标
	FVector2D    Location;
	FGameplayTag HitZone;
	bool         bShowAsSuccess = false;
};

/** 一次射击产生的、等待服务器确认的命中标记集合。 */
struct FServerSideHitMarkerBatch
{
	FServerSideHitMarkerBatch() {}

	explicit FServerSideHitMarkerBatch(uint8 InUniqueId)
		: UniqueId(InUniqueId)
	{}

	TArray<FScreenSpaceHitLocation> Markers;

	// 与 FGameplayAbilityTargetDataHandle::UniqueId 对应，用于回执配对
	uint8 UniqueId = 0;
};

/**
 * 追踪武器状态与近期已确认的命中标记，供准星表现读取。
 * 客户端先记录预测命中，服务器回执后才转为可显示的命中反馈。
 */
UCLASS(MinimalAPI)
class UWeaponStateComponent : public UControllerComponent
{
	GENERATED_BODY()

public:
	UE_API UWeaponStateComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	//~Begin UActorComponent Interface
	UE_API virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	//~End UActorComponent Interface

	/** 服务器回执：确认某批命中是否成立，HitReplaces 中的下标为被否决的命中 */
	UFUNCTION(Client, Reliable)
	UE_API void ClientConfirmTargetData(uint16 UniqueId, bool bSuccess, const TArray<uint8>& HitReplaces);

	/** 记录一批尚未被服务器确认的命中 */
	UE_API void AddUnconfirmedServerSideHitMarkers(const FGameplayAbilityTargetDataHandle& InTargetData, const TArray<FHitResult>& FoundHits);

	/** 刷新本控制器最近一次造成伤害的时间 */
	UE_API void UpdateDamageInstigatedTime(const FGameplayEffectContextHandle& EffectContext);

	/** 取最近一批已确认命中的屏幕空间坐标 */
	void GetLastWeaponDamageScreenLocations(TArray<FScreenSpaceHitLocation>& WeaponDamageScreenLocations) const
	{
		WeaponDamageScreenLocations = LastWeaponDamageScreenLocations;
	}

	/** 距上一次造成伤害经过的时间 */
	UE_API double GetTimeSinceLastHitNotification() const;

	int32 GetUnconfirmedServerSideHitMarkerCount() const { return UnconfirmedServerSideHitMarkers.Num(); }

protected:
	// 判定一次命中是否算「成功」：默认为命中了可对其造成伤害的目标
	UE_API virtual bool ShouldShowHitAsSuccess(const FHitResult& Hit) const;

	UE_API virtual bool ShouldUpdateDamageInstigatedTime(const FGameplayEffectContextHandle& EffectContext) const;

	UE_API void ActuallyUpdateDamageInstigatedTime();

private:
	// 最近一次造成武器伤害的时间
	double LastWeaponDamageInstigatedTime = 0.0;

	// 已确认命中的屏幕空间坐标
	TArray<FScreenSpaceHitLocation> LastWeaponDamageScreenLocations;

	// 等待服务器确认的命中批次
	TArray<FServerSideHitMarkerBatch> UnconfirmedServerSideHitMarkers;
};

#undef UE_API
