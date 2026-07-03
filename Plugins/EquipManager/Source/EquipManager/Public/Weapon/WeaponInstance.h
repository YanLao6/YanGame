// 武器实例声明。

#pragma once

#include "CoreMinimal.h"
#include "Cosmetics/ModularCosmeticAnimationTypes.h"
#include "Equipment/EquipmentInstance.h"
#include "WeaponInstance.generated.h"

#define UE_API EQUIPMANAGER_API

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
	// 记录当前武器已经激活的输入设备属性句柄，便于卸下时统一清理。
	UPROPERTY(Transient)
	TSet<FInputDevicePropertyHandle> DevicePropertyHandles;

	// 最近一次装备武器的世界时间。
	double TimeLastEquipped = 0.0;
	// 最近一次触发武器开火的世界时间。
	double TimeLastFired    = 0.0;
};

#undef UE_API
