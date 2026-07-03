// Copyright (c) 2026 LazyDevelopers <lazydeveloper24@gmail.com>. All rights reserved.
// This plugin is distributed under the Fab Standard License.
//
// This product was independently developed by us while participating in the Epic Project, a developer-support
// program of the KRAFTON JUNGLE GameTech Lab. All rights, title, and interest in and to the product are exclusively
// vested in us. Krafton, Inc. was not involved in its development and distribution and disclaims all representations
// and warranties, express or implied, and assumes no responsibility or liability for any consequences arising from
// the use of this product.


#include "Actors/AnchorActor.h"


// 设置默认值
AAnchorActor::AAnchorActor()
{
 	// 将此 actor 设置为每帧调用 Tick()。如果您不需要它，可以将其关闭以提高性能。
	PrimaryActorTick.bCanEverTick = false;
	bIsEditorOnlyActor = true;

}

// 当游戏开始或生成时调用
void AAnchorActor::BeginPlay()
{
	Super::BeginPlay();
	
}

// 每帧调用
void AAnchorActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

void AAnchorActor::ApplyToAnchors(const FTransform& MeshTransform, FGridCellLayout& CellCache)
{
}

