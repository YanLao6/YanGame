// 装备定义声明。

#pragma once

#include "CoreMinimal.h"
#include "GameplayAbilities/ModularAbilitySet.h"
#include "UObject/Object.h"
#include "EquipmentDefinition.generated.h"


class UEquipmentInstance;

/**
 * 装备附属 Actor 的生成配置。
 *
 * 描述装备实例在进入已装备状态后需要额外创建并附着的 Actor，常用于武器模型、特效挂件等对象。
 */
USTRUCT()
struct FEquipmentActorToSpawn
{
	GENERATED_BODY()

	FEquipmentActorToSpawn()
	{}

	/** 需要生成的 Actor 类。 */
	UPROPERTY(EditAnywhere, Category=Equipment)
	TSubclassOf<AActor> ActorToSpawn;

	/** 生成后附着到目标组件时使用的插槽名称。 */
	UPROPERTY(EditAnywhere, Category=Equipment)
	FName AttachSocket;

	/** 生成后相对于附着插槽使用的局部变换。 */
	UPROPERTY(EditAnywhere, Category=Equipment)
	FTransform AttachTransform;
};

/**
 * 装备定义资源。
 *
 * 用于描述某个可装备对象在运行时应创建哪种装备实例，以及装备时需要派生出的附属 Actor。
 */
UCLASS(MinimalAPI, Blueprintable, Const, Abstract, BlueprintType)
class UEquipmentDefinition : public UObject
{
	GENERATED_BODY()

public:
	/** 构造装备定义对象并初始化默认实例类型。 */
	UEquipmentDefinition(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** 装备后创建的运行时实例类型；未指定时回退到基础装备实例类型。 */
	UPROPERTY(EditDefaultsOnly, Category=Equipment)
	TSubclassOf<UEquipmentInstance> InstanceType;

	// 装备生效时准备授予的能力集合。
	UPROPERTY(EditDefaultsOnly, Category=Equipment)
	TArray<TObjectPtr<const UModularAbilitySet>> AbilitySetsToGrant;

	/** 装备成功后需要同步生成并附着的附属 Actor 列表。 */
	UPROPERTY(EditDefaultsOnly, Category=Equipment)
	TArray<FEquipmentActorToSpawn> ActorsToSpawn;
};
