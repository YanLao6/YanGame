#pragma once

#include "Abilities/YanGameplayAbility.h"

#include "YanLoadVisualsAbility.generated.h"

#define UE_API YANGAMEPLAY_API

class UMeshComponent;

/**
 * 负荷表现被动技能：把负荷值映射到角色材质参数。
 *
 * 监听走 ASC 的属性变化委托而非属性集的 OnRep：OnRep 只在客户端触发，
 * listen server 的主机端永远收不到，自己的颜色会一直停在初始值。
 *
 * 只写材质标量参数、不换材质资产，负荷是连续量，离散换材质会丢掉中间段的信息；
 * 需要分档表现时由材质内部对该参数做阶梯即可，无须改动本类。
 */
UCLASS(MinimalAPI)
class UYanLoadVisualsAbility : public UYanGameplayAbility
{
	GENERATED_BODY()

public:
	UE_API explicit UYanLoadVisualsAbility(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	//~Begin UGameplayAbility Interface
	UE_API virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	UE_API virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;
	//~End UGameplayAbility Interface

protected:
	/** 接收负荷比例（0–1）的材质标量参数名。 */
	UPROPERTY(EditDefaultsOnly, Category = "Visuals")
	FName LoadRatioParameterName = FName("LoadRatio");

private:
	// 属性变化回调，重算比例并下发到材质
	UE_API void HandleLoadChanged(const struct FOnAttributeChangeData& ChangeData);

	// 按当前负荷刷新全部动态材质实例
	UE_API void PushLoadRatioToMaterials(float LoadRatio) const;

	// 收集 Avatar 上的网格体组件，惰性创建其动态材质实例
	UE_API void CacheMeshComponents();

	UPROPERTY(Transient)
	TArray<TObjectPtr<UMeshComponent>> CachedMeshes;

	FDelegateHandle LoadChangedDelegateHandle;
};

#undef UE_API
