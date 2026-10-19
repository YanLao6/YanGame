// 武器刷新台声明。

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Templates/SubclassOf.h"
#include "WeaponSpawner.generated.h"

#define UE_API EQUIPMANAGER_API

namespace EEndPlayReason { enum Type : int; }

class APawn;
class UCapsuleComponent;
class UInventoryItemDefinition;
class UPrimitiveComponent;
class UStaticMeshComponent;
class UWeaponPickupDefinition;
struct FGameplayTag;
struct FHitResult;

/**
 * 武器刷新台。
 *
 * 放置在关卡中的定点武器补给：Pawn 走入碰撞体积即尝试发放武器，
 * 发放成功后进入冷却，冷却结束再次可用。
 * 实际的发放动作由蓝图实现，以便接入具体项目的背包与授予流程。
 */
UCLASS(MinimalAPI, Blueprintable, BlueprintType)
class AWeaponSpawner : public AActor
{
	GENERATED_BODY()

public:
	UE_API AWeaponSpawner();

protected:
	//~Begin AActor Interface
	UE_API virtual void BeginPlay() override;
	UE_API virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	//~End AActor Interface

public:
	//~Begin AActor Interface
	UE_API virtual void Tick(float DeltaTime) override;
	UE_API virtual void OnConstruction(const FTransform& Transform) override;
	//~End AActor Interface

protected:
	/** 本刷新台发放的武器配置；按关卡实例逐个指定。 */
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "EM|WeaponPickup")
	TObjectPtr<UWeaponPickupDefinition> WeaponDefinition;

	/** 武器当前是否可被拾取。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, ReplicatedUsing = OnRep_WeaponAvailability, Category = "EM|WeaponPickup")
	bool bIsWeaponAvailable;

	/** 拾取后到再次刷新的间隔，单位为秒。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "EM|WeaponPickup")
	float CoolDownTime;

	/** 刷新后延迟多久再检查台上是否已站有 Pawn；留给可用状态的同步与特效播放。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "EM|WeaponPickup")
	float CheckExistingOverlapDelay;

	/** 冷却进度，取值 0 到 1，用于驱动刷新倒计时的表现。 */
	UPROPERTY(BlueprintReadOnly, Transient, Category = "EM|WeaponPickup")
	float CoolDownPercentage;

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "EM|WeaponPickup")
	TObjectPtr<UCapsuleComponent> CollisionVolume;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "EM|WeaponPickup")
	TObjectPtr<UStaticMeshComponent> PadMesh;

	UPROPERTY(BlueprintReadOnly, Category = "EM|WeaponPickup")
	TObjectPtr<UStaticMeshComponent> WeaponMesh;

	/** 展示网格的自转速度，单位为度每秒。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "EM|WeaponPickup")
	float WeaponMeshRotationSpeed;

	FTimerHandle CoolDownTimerHandle;

	FTimerHandle CheckOverlapsDelayTimerHandle;

	/** 对进入体积的 Pawn 尝试发放武器；仅在服务端且武器可用时生效。 */
	UFUNCTION(BlueprintNativeEvent)
	UE_API void AttemptPickUpWeapon(APawn* Pawn);

	/**
	 * 把武器授予目标 Pawn，返回是否授予成功。
	 * 由蓝图实现，以便对接具体项目的背包与授予流程。
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "EM|WeaponPickup")
	UE_API bool GiveWeapon(TSubclassOf<UInventoryItemDefinition> WeaponItemClass, APawn* ReceivingPawn);

	/** 立即结束冷却并恢复可拾取状态。 */
	UFUNCTION(BlueprintCallable, Category = "EM|WeaponPickup")
	UE_API void ResetCoolDown();

	/** 拾取瞬间的音效与特效。 */
	UFUNCTION(BlueprintNativeEvent, Category = "EM|WeaponPickup")
	UE_API void PlayPickupEffects();

	/** 武器刷新回场景时的音效与特效。 */
	UFUNCTION(BlueprintNativeEvent, Category = "EM|WeaponPickup")
	UE_API void PlayRespawnEffects();

	/** 从物品定义的初始统计值片段中读取指定 Tag 的数值；未配置时返回 0。 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "EM|WeaponPickup")
	static UE_API int32 GetDefaultStatFromItemDef(const TSubclassOf<UInventoryItemDefinition> WeaponItemClass, FGameplayTag StatTag);

	UE_API void StartCoolDown();

	UE_API void SetWeaponPickupVisibility(bool bShouldBeVisible);

private:
	UFUNCTION()
	UE_API void OnOverlapBegin(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepHitResult);

	// 刷新后补检台上已存在的 Pawn，避免站着不动的玩家拿不到武器
	void CheckForExistingOverlaps();

	UFUNCTION()
	void OnCoolDownTimerComplete();

	UFUNCTION()
	void OnRep_WeaponAvailability();
};

#undef UE_API
