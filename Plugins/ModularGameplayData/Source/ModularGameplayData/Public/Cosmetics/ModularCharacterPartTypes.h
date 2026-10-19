// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "ModularCharacterPartTypes.generated.h"

struct FModularCharacterPartList;

/** 部件生成后如何处理其碰撞。 */
UENUM()
enum class EModularCharacterPartCollisionMode : uint8
{
	// 关闭部件上所有图元组件的碰撞
	NoCollision,

	// 保留部件资产自身的碰撞设置
	UseCollisionFromCharacterPart
};

/**
 * 添加部件时返回的句柄，用于之后移除该部件。
 * 句柄仅在服务端有意义，客户端上的部件由复制驱动。
 */
USTRUCT(BlueprintType)
struct FModularCharacterPartHandle
{
	GENERATED_BODY()

	void Reset() { PartHandle = INDEX_NONE; }

	bool IsValid() const { return PartHandle != INDEX_NONE; }

private:
	UPROPERTY()
	int32 PartHandle = INDEX_NONE;

	friend FModularCharacterPartList;
};

/** 一次部件挂载请求。 */
USTRUCT(BlueprintType)
struct FModularCharacterPart
{
	GENERATED_BODY()

	// 要生成的部件 Actor 类
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSubclassOf<AActor> PartClass;

	// 挂载到的插槽，留空则挂在组件本身
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FName SocketName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EModularCharacterPartCollisionMode CollisionMode = EModularCharacterPartCollisionMode::NoCollision;

	/** 比较两个部件是否等价，忽略碰撞模式。 */
	static bool AreEquivalentParts(const FModularCharacterPart& A, const FModularCharacterPart& B)
	{
		return (A.PartClass == B.PartClass) && (A.SocketName == B.SocketName);
	}
};
