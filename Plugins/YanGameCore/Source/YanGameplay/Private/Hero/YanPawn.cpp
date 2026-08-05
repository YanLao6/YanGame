// Fill out your copyright notice in the Description page of Project Settings.


#include "Hero/YanPawn.h"

#include "GameFramework/GameplayCameraComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(YanPawn)


AYanPawn::AYanPawn(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	Capsule = CreateDefaultSubobject<UCapsuleComponent>(TEXT("Capsule"));
	Capsule->SetCollisionProfileName(TEXT("Pawn"));
	Capsule->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	Capsule->SetSimulatePhysics(true);
	Capsule->SetEnableGravity(true);
	SetRootComponent(Capsule);

	// Mover 用 NetworkPrediction 自行同步位置，禁用引擎默认的 ReplicateMovement 避免冲突
	AActor::SetReplicateMovement(false);
}
