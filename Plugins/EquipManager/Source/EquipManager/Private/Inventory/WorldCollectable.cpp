// Copyright Epic Games, Inc. All Rights Reserved.

#include "Inventory/WorldCollectable.h"

#include "Components/SphereComponent.h"
#include "Interaction/InteractionChannels.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(WorldCollectable)

struct FInteractionQuery;

AWorldCollectable::AWorldCollectable()
{
	// 交互检测碰撞体：仅参与查询，默认忽略所有通道，仅对 Interaction 通道 Overlap。
	InteractionCollision = CreateDefaultSubobject<USphereComponent>(TEXT("InteractionCollision"));
	InteractionCollision->InitSphereRadius(100.0f);
	InteractionCollision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	InteractionCollision->SetCollisionResponseToAllChannels(ECR_Ignore);
	InteractionCollision->SetCollisionResponseToChannel(EquipManager_TraceChannel_Interaction, ECR_Overlap);
	SetRootComponent(InteractionCollision);
}

void AWorldCollectable::GatherInteractionOptions(const FInteractionQuery& InteractQuery, FInteractionOptionBuilder& InteractionBuilder)
{
	InteractionBuilder.AddInteractionOption(Option);
}

FInventoryPickup AWorldCollectable::GetPickupInventory() const
{
	return StaticInventory;
}
