// 装备实例声明。

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "EquipmentInstance.generated.h"

#define UE_API EQUIPMANAGER_API

struct FEquipmentActorToSpawn;

/**
 * 装备运行时实例。
 *
 * 每次角色装备某个装备定义时都会生成一个实例，用于保存运行时状态、生成附属 Actor，
 * 并向蓝图暴露装备生命周期事件。
 */
UCLASS(MinimalAPI, Blueprintable,BlueprintType)
class UEquipmentInstance : public UObject
{
	GENERATED_BODY()

public:
	/** 构造装备实例对象。 */
	UEquipmentInstance(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	//~Begin UObject Interface
	// 声明该对象支持网络复制。
	virtual bool IsSupportedForNetworking() const override { return true; }
	// 返回当前装备实例所属的世界，通常通过拥有它的 Pawn 获取。
	virtual UWorld* GetWorld() const override final;
	//~End UObject Interface

	/** 返回触发本次装备创建的来源对象，通常是某个物品实例。 */
	UFUNCTION(BlueprintPure, Category=Equipment)
	UObject* GetInstigator() const { return Instigator; }

	/** 设置本次装备实例的来源对象。 */
	void SetInstigator(UObject* InInstigator) { Instigator = InInstigator; }

	/** 返回拥有当前装备实例的 Pawn。 */
	UFUNCTION(BlueprintPure, Category=Equipment)
	APawn* GetPawn() const;

	/** 按指定 Pawn 类型返回拥有者，类型不匹配时返回空。 */
	UFUNCTION(BlueprintPure, Category=Equipment, meta=(DeterminesOutputType=PawnType))
	APawn* GetTypedPawn(TSubclassOf<APawn> PawnType) const;

	/** 返回当前装备实例已生成并仍由其管理的全部附属 Actor。 */
	UFUNCTION(BlueprintPure, Category=Equipment)
	TArray<AActor*> GetSpawnedActors() const { return SpawnedActors; }

	/** 按装备定义配置生成并附着附属 Actor。 */
	virtual void SpawnEquipmentActors(const TArray<FEquipmentActorToSpawn>& ActorsToSpawn);
	/** 销毁当前装备实例管理的全部附属 Actor。 */
	virtual void DestroyEquipmentActors();

	/**
	 * 设置附属 Actor 的显隐。
	 *
	 * 附属 Actor 自实例创建起就常驻，切换装备只改显隐而不重新生成，
	 * 因此切换过程不触碰任何复制状态，客户端可先行切换。
	 * 期望值会被记住并在附属 Actor 复制到达时重新应用，故本函数可早于 Actor 到达调用。
	 */
	UE_API void SetEquipmentActorsHidden(bool bHidden);

	/** 装备进入已装备状态时调用。 */
	virtual void OnEquipped();
	/** 装备离开已装备状态时调用。 */
	virtual void OnUnequipped();

	/**
	 * 重新应用本实例的外观表现。
	 *
	 * 动画层之类的外观会被拥有者网格的重建整体抹除，故外观须可在任意时刻重放，
	 * 而不能只在装备那一刻应用一次。本调用幂等：外观已与期望一致时不做任何事。
	 * 应用成什么样取决于 IsEquipped，未生效的实例同样需要重放。
	 */
	UFUNCTION(BlueprintCallable, Category=Equipment)
	UE_API virtual void RefreshCosmetics();

	/** 返回本实例当前是否处于生效状态。 */
	UFUNCTION(BlueprintPure, Category=Equipment)
	bool IsEquipped() const { return bActivated; }

protected:
	//~Begin UObject Interface
	// 注册 Iris 复制所需的属性片段。
	virtual void RegisterReplicationFragments(UE::Net::FFragmentRegistrationContext& Context, UE::Net::EFragmentRegistrationFlags RegistrationFlags) override;
	//~End UObject Interface

	/** 蓝图事件：当装备被成功装备时触发。 */
	UFUNCTION(BlueprintImplementableEvent, Category=Equipment, meta=(DisplayName="OnEquipped"))
	void K2_OnEquipped();

	/** 蓝图事件：当装备被卸下时触发。 */
	UFUNCTION(BlueprintImplementableEvent, Category=Equipment, meta=(DisplayName="OnUnequipped"))
	void K2_OnUnequipped();

	/**
	 * 蓝图事件：重新应用外观表现时触发。
	 *
	 * 与 OnEquipped 不同，本事件会被重复触发，故其中只应放置幂等的表现逻辑，
	 * 播放蒙太奇、发出音效之类的一次性表现仍应留在 OnEquipped。
	 */
	UFUNCTION(BlueprintImplementableEvent, Category=Equipment, meta=(DisplayName="RefreshCosmetics"))
	void K2_RefreshCosmetics();

private:
	// Instigator 在客户端完成复制后触发。
	UFUNCTION()
	void OnRep_Instigator();

	// 附属 Actor 复制到达后重新应用期望显隐，避免其在生效状态设定之后才到达而显错。
	UFUNCTION()
	void OnRep_SpawnedActors();

	// 将当前期望显隐应用到已有的全部附属 Actor。
	void ApplyEquipmentActorsVisibility();

private:
	// 指向触发本次装备实例创建的来源对象。
	UPROPERTY(ReplicatedUsing=OnRep_Instigator)
	TObjectPtr<UObject> Instigator;

	// 由当前装备实例生成并负责销毁的附属 Actor 列表。
	UPROPERTY(ReplicatedUsing=OnRep_SpawnedActors)
	TArray<TObjectPtr<AActor>> SpawnedActors;

	// 期望的附属 Actor 显隐；不复制，两端各自依本地生效状态维护。实例创建后未生效，故默认隐藏。
	bool bEquipmentActorsHidden = true;

	// 本端的生效状态；不复制，与生效意图的解析结果一同维护。外观重放据此决定应用成什么样。
	bool bActivated = false;
};

#undef UE_API
