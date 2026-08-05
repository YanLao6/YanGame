// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "NativeGameplayTags.h"

#include "ModularNotificationMessage.generated.h"

#define UE_API MODULARGAMEPLAYEXPERIENCES_API

class UObject;

MODULARGAMEPLAYEXPERIENCES_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Modular_AddNotification_Message);

class APlayerState;

/**
 * 发往临时通知流的消息（如淘汰 Feed、拾取日志等 UI Channel）。
 */
USTRUCT(BlueprintType)
struct FModularNotificationMessage
{
	GENERATED_BODY()

	/** 目标 UI Channel（GameplayTag）。 */
	UPROPERTY(BlueprintReadWrite, Category=Notification)
	FGameplayTag TargetChannel;

	/** 目标玩家；空表示对所有本地玩家显示。 */
	UPROPERTY(BlueprintReadWrite, Category=Notification)
	TObjectPtr<APlayerState> TargetPlayer = nullptr;

	/** 展示用 FText。 */
	UPROPERTY(BlueprintReadWrite, Category=Notification)
	FText PayloadMessage;

	/** Channel 相关的附加 Tag（如样式）。 */
	UPROPERTY(BlueprintReadWrite, Category=Notification)
	FGameplayTag PayloadTag;

	/** Channel 相关的附加 UObject（如定义资产）。 */
	UPROPERTY(BlueprintReadWrite, Category=Notification)
	TObjectPtr<UObject> PayloadObject = nullptr;
};

#undef UE_API
