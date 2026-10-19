// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Equipment/Abilities/GameplayAbility_FromEquipment.h"

#include "GameplayAbility_RangedWeapon.generated.h"

#define UE_API EQUIPMANAGER_API

enum ECollisionChannel : int;

class APawn;
class URangedWeaponInstance;
struct FCollisionQueryParams;
struct FGameplayAbilityActorInfo;
struct FGameplayEventData;
struct FGameplayTag;
struct FGameplayTagContainer;

/** 定义能力从哪里开始检测、朝向哪里。 */
UENUM(BlueprintType)
enum class EWeaponTargetingSource : uint8
{
	// 从玩家相机出发，指向相机焦点
	CameraTowardsFocus,
	// 从 Pawn 中心出发，沿 Pawn 朝向
	PawnForward,
	// 从 Pawn 中心出发，指向相机焦点
	PawnTowardsFocus,
	// 从武器枪口出发，沿 Pawn 朝向
	WeaponForward,
	// 从武器枪口出发，指向相机焦点
	WeaponTowardsFocus,
	// 由蓝图自行指定的来源位置
	Custom
};

/**
 * 远程武器射击能力。
 *
 * 只负责瞄准与命中检测：在本地控制端解算射线、按散布采样弹道、生成 TargetData 并同步给服务器，
 * 命中之后的伤害与表现交由蓝图在 OnRangedWeaponTargetDataReady 中施加 GameplayEffect 完成。
 */
UCLASS(MinimalAPI)
class UGameplayAbility_RangedWeapon : public UGameplayAbility_FromEquipment
{
	GENERATED_BODY()

public:

	UE_API UGameplayAbility_RangedWeapon(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** 返回授予该能力的远程武器实例；装备类型不匹配时返回空。 */
	UFUNCTION(BlueprintCallable, Category = "Weapon|Ability")
	UE_API URangedWeaponInstance* GetWeaponInstance() const;

	//~Begin UGameplayAbility Interface
	UE_API virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags = nullptr, const FGameplayTagContainer* TargetTags = nullptr, OUT FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;
	UE_API virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	UE_API virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;
	//~End UGameplayAbility Interface

protected:
	// 单次击发所需的全部瞄准输入
	struct FRangedWeaponFiringInput
	{
		// 检测起点
		FVector StartTrace;

		// 完全精准时的检测终点
		FVector EndAim;

		// 完全精准时的检测方向
		FVector AimDir;

		// 武器实例，射击参数的数据来源
		URangedWeaponInstance* WeaponData = nullptr;

		// 本次检测是否允许播放子弹表现
		bool bCanPlayBulletFX = false;

		FRangedWeaponFiringInput()
			: StartTrace(ForceInitToZero)
			, EndAim(ForceInitToZero)
			, AimDir(ForceInitToZero)
		{
		}
	};

protected:
	/** 开始一次射击瞄准，由蓝图在开火时机调用。 */
	UFUNCTION(BlueprintCallable)
	UE_API void StartRangedWeaponTargeting();

	/** 命中数据就绪，在此对目标施加 GameplayEffect 与表现。 */
	UFUNCTION(BlueprintImplementableEvent)
	UE_API void OnRangedWeaponTargetDataReady(const FGameplayAbilityTargetDataHandle& TargetData);
	
	// 返回命中结果中第一个属于 Pawn（或挂在 Pawn 上）的下标，没有则返回 INDEX_NONE
	static UE_API int32 FindFirstPawnHitResult(const TArray<FHitResult>& HitResults);

	// 执行一次武器检测；SweepRadius 大于零时使用球体扫掠，否则使用线性射线
	UE_API FHitResult WeaponTrace(const FVector& StartTrace, const FVector& EndTrace, float SweepRadius, bool bIsSimulated, OUT TArray<FHitResult>& OutHitResults) const;

	// 先尝试线性射线，未命中 Pawn 且 SweepRadius 大于零时再退化为球体扫掠
	UE_API FHitResult DoSingleBulletTrace(const FVector& StartTrace, const FVector& EndTrace, float SweepRadius, bool bIsSimulated, OUT TArray<FHitResult>& OutHits) const;

	// 检测单发弹药中的所有子弹
	UE_API void TraceBulletsInCartridge(const FRangedWeaponFiringInput& InputData, OUT TArray<FHitResult>& OutHits);

	/** 追加需要在武器检测中忽略的 Actor，子类可扩展。 */
	UE_API virtual void AddAdditionalTraceIgnoreActors(FCollisionQueryParams& TraceParams) const;

	/** 决定武器检测使用的碰撞通道，子类可扩展。 */
	UE_API virtual ECollisionChannel DetermineTraceChannel(FCollisionQueryParams& TraceParams, bool bIsSimulated) const;

	/** 仅在本地控制端解算瞄准并产出命中结果；子类可替换整套瞄准解算。 */
	UE_API virtual void PerformLocalTargeting(OUT TArray<FHitResult>& OutHits);

	// 返回武器射击的起始位置
	UE_API FVector GetWeaponTargetingSourceLocation() const;

	// 按指定来源模式解算瞄准位置与朝向
	UE_API FTransform GetTargetingTransform(APawn* SourcePawn, EWeaponTargetingSource Source) const;

	/**
	 * TargetData 就绪后的处理：同步给服务器、结算消耗并抛出蓝图事件。
	 *
	 * 子类重写时注意：本实现会取走 InData 的所有权，需要的数据必须在调用父类之前拷贝出来。
	 */
	UE_API virtual void OnTargetDataReadyCallback(const FGameplayAbilityTargetDataHandle& InData, FGameplayTag ApplicationTag);

private:
	FDelegateHandle OnTargetDataReadyCallbackDelegateHandle;
};

#undef UE_API
