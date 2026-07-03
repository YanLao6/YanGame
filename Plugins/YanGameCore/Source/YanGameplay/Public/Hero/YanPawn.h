// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Actor/ModularExperiencePawn.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/GameplayCameraComponent.h"
#include "YanPawn.generated.h"

/**
 * 项目基础 Pawn，集成 Mover 运动系统。
 * 移动输入到 Mover 的桥接由 UYanHeroInputComponent 实现（IMoverInputProducerInterface），
 * Pawn 本身无需感知 Mover 的存在。
 */
UCLASS()
class YANGAMEPLAY_API AYanPawn : public AModularExperiencePawn
{
	GENERATED_BODY()

public:
	/** 构造：创建胶囊体根组件与 GameplayCamera。 */
	explicit AYanPawn(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TObjectPtr<UCapsuleComponent> Capsule;
};
