// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GameFramework/Actor.h"
#include "Interaction/IInteractableTarget.h"
#include "Interaction/InteractionOption.h"
#include "Inventory/IPickupable.h"

#include "WorldCollectable.generated.h"

class UObject;
class USphereComponent;
struct FInteractionQuery;

/**
 * 世界可拾取物基类。
 *
 * 放置于场景中的可交互 / 可拾取 Actor：靠近时提供交互选项，拾取时向背包注入 StaticInventory。
 * 自带一个默认对 Interaction 通道 Overlap 的碰撞体，可直接被交互扫描命中。
 */
UCLASS(MinimalAPI, Abstract, Blueprintable)
class AWorldCollectable : public AActor, public IInteractableTarget, public IPickupable
{
	GENERATED_BODY()

public:

	AWorldCollectable();

	//~Begin IInteractableTarget Interface
	virtual void GatherInteractionOptions(const FInteractionQuery& InteractQuery, FInteractionOptionBuilder& InteractionBuilder) override;
	//~End IInteractableTarget Interface

	//~Begin IPickupable Interface
	virtual FInventoryPickup GetPickupInventory() const override;
	//~End IPickupable Interface

protected:
	/** 交互选项：在此配置 InteractionAbilityToGrant 指向拾取能力。 */
	UPROPERTY(EditAnywhere)
	FInteractionOption Option;

	/** 拾取后注入背包的内容（在 Templates 中填入 ItemDef 与数量）。 */
	UPROPERTY(EditAnywhere)
	FInventoryPickup StaticInventory;

	/** 交互检测碰撞体，默认对 Interaction 通道 Overlap，供交互扫描命中。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Collectable)
	TObjectPtr<USphereComponent> InteractionCollision;
};
