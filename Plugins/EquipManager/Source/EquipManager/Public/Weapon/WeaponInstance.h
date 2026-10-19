// 武器实例声明。

#pragma once

#include "CoreMinimal.h"
#include "Cosmetics/ModularCosmeticAnimationTypes.h"
#include "Equipment/EquipmentInstance.h"
#include "WeaponInstance.generated.h"

#define UE_API EQUIPMANAGER_API

class USkeletalMeshComponent;

/**
 * 武器装备实例。
 *
 * 在基础装备实例之上扩展了动画层选择、交互时间记录以及输入设备属性控制能力。
 */
UCLASS(MinimalAPI)
class UWeaponInstance : public UEquipmentInstance
{
	GENERATED_BODY()

public:
	/** 构造武器实例对象。 */
	UE_API UWeaponInstance(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	//~Begin UEquipmentInstance Interface
	// 武器被装备时刷新交互时间并应用输入设备属性。
	UE_API virtual void OnEquipped() override;
	// 武器被卸下时移除输入设备属性。
	UE_API virtual void OnUnequipped() override;
	// 依当前装备状态与外观标签，把应有的动画层重新链接到拥有者主网格。
	UE_API virtual void RefreshCosmetics() override;
	//~End UEquipmentInstance Interface

	/** 记录武器最近一次开火时间。 */
	UFUNCTION(BlueprintCallable)
	UE_API void UpdateFiringTime();

	/** 返回距离武器最近一次被交互的时间，交互包括装备和开火。 */
	UFUNCTION(BlueprintPure)
	UE_API float GetTimeSinceLastInteractedWith() const;

protected:
	/** 武器处于装备状态时可选用的动画层集合。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Animation)
	FModularAnimLayerSelectionSet EquippedAnimSet;

	/** 武器处于未装备状态时可选用的动画层集合。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Animation)
	FModularAnimLayerSelectionSet UneuippedAnimSet;
	
	/**
	 * 武器装备期间需要应用到输入设备上的属性列表。
	 * 这些属性会以循环模式生效，直到武器被卸下时才会被主动移除。
	 */
	UPROPERTY(EditDefaultsOnly, Instanced, BlueprintReadOnly, Category = "Input Devices")
	TArray<TObjectPtr<UInputDeviceProperty>> ApplicableDeviceProperties;

	/** 根据是否已装备以及外观标签，从对应动画层集合中选择最匹配的动画层。 */
	UFUNCTION(BlueprintCallable, BlueprintPure=false, Category=Animation)
	UE_API TSubclassOf<UAnimInstance> PickBestAnimLayer(bool bEquipped, const FGameplayTagContainer& CosmeticTags) const;
	
	/** 返回拥有该武器实例的 Pawn 所属平台用户 ID。 */
	UFUNCTION(BlueprintCallable)
	UE_API const FPlatformUserId GetOwningUserId() const;

	// 当拥有该武器的 Pawn 死亡时触发，确保清理已激活的设备属性。
	UFUNCTION()
	UE_API void OnDeathStarted(AActor* OwningActor);

	// 将配置的输入设备属性应用到拥有该武器的玩家输入设备上。
	UE_API void ApplyDeviceProperties();

	// 移除在 ApplyDeviceProperties 中激活过的全部输入设备属性。
	UE_API void RemoveDeviceProperties();

private:
	// 选出应有的动画层并链接到拥有者主网格；与已链接内容一致时不做任何事。
	UE_API void ApplyAnimLayer();

	// 收集拥有者的外观标签，取自其角色部件组件；没有该组件时为空。
	UE_API FGameplayTagContainer DetermineCosmeticTags() const;

	// 取拥有者主网格，即首个骨骼网格组件。
	UE_API USkeletalMeshComponent* GetOwningMesh() const;

private:
	// 记录当前武器已经激活的输入设备属性句柄，便于卸下时统一清理。
	UPROPERTY(Transient)
	TSet<FInputDevicePropertyHandle> DevicePropertyHandles;

	/**
	 * 本实例链接上去的动画层。
	 *
	 * 只用于知道该摘掉哪一层；链接当前是否还成立一律向网格求证，
	 * 因为动画实例重新初始化会清空链接而本记录不会随之失效。
	 */
	UPROPERTY(Transient)
	TSubclassOf<UAnimInstance> LinkedAnimLayer;

	// 最近一次装备武器的世界时间。
	double TimeLastEquipped = 0.0;
	// 最近一次触发武器开火的世界时间。
	double TimeLastFired    = 0.0;
};

#undef UE_API
