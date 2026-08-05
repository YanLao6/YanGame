// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "Templates/SubclassOf.h"
#include "UObject/Interface.h"

#include "UObject/ObjectPtr.h"
#include "IPickupable.generated.h"

#define UE_API EQUIPMANAGER_API

template <typename InterfaceType>

class TScriptInterface;

class AActor;
class UInventoryItemDefinition;
class UInventoryItemInstance;
class UInventoryManagerComponent;
class UObject;
struct FFrame;

/** 按物品定义与数量描述的拾取模板。 */
USTRUCT(BlueprintType)
struct FPickupTemplate
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere)
	int32 StackCount = 1;

	UPROPERTY(EditAnywhere)
	TSubclassOf<UInventoryItemDefinition> ItemDef;
};

/** 以已存在实例描述的拾取项。 */
USTRUCT(BlueprintType)
struct FPickupInstance
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TObjectPtr<UInventoryItemInstance> Item = nullptr;
};

/** 一次拾取所包含的全部内容（模板与实例两类）。 */
USTRUCT(BlueprintType)
struct FInventoryPickup
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TArray<FPickupInstance> Instances;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TArray<FPickupTemplate> Templates;
};

UINTERFACE(MinimalAPI, BlueprintType, meta = (CannotImplementInterfaceInBlueprint))
class UPickupable : public UInterface
{
	GENERATED_BODY()
};

/** 可拾取接口：实现者对外提供其可被拾取加入背包的内容。 */
class IPickupable
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable)
	virtual FInventoryPickup GetPickupInventory() const = 0;
};

/** 可拾取相关的静态工具库。 */
UCLASS(MinimalAPI)
class UPickupableStatics: public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UE_API UPickupableStatics();

public:
	/** 从 Actor 上获取第一个可拾取接口（Actor 自身或其组件）。 */
	UFUNCTION(BlueprintPure)
	static UE_API TScriptInterface<IPickupable> GetFirstPickupableFromActor(const AActor* Actor);

	/** 将可拾取内容加入指定背包组件，仅服务器可调用。 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, meta = (WorldContext = "Ability"))
	static UE_API void AddPickupToInventory(UInventoryManagerComponent* InventoryComponent, TScriptInterface<IPickupable> Pickup);
};

#undef UE_API
