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

	/** 装备进入已装备状态时调用。 */
	virtual void OnEquipped();
	/** 装备离开已装备状态时调用。 */
	virtual void OnUnequipped();

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

private:
	// Instigator 在客户端完成复制后触发。
	UFUNCTION()
	void OnRep_Instigator();

private:
	// 指向触发本次装备实例创建的来源对象。
	UPROPERTY(ReplicatedUsing=OnRep_Instigator)
	TObjectPtr<UObject> Instigator;

	// 由当前装备实例生成并负责销毁的附属 Actor 列表。
	UPROPERTY(Replicated)
	TArray<TObjectPtr<AActor>> SpawnedActors;
};

#undef UE_API
