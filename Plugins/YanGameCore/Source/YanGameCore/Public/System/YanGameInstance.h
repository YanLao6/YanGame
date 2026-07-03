// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "ModularGameInstance.h"

#include "YanGameInstance.generated.h"

/**
 * 游戏实例（GameInstance）
 *
 * 说明：
 * - 本项目使用 ModularGameplay 的 `UModularGameInstance` 作为基类。
 * - 同时集成 CommonGame 的 UI 管理链路：当本地玩家（LocalPlayer）创建/销毁时，通知 `UGameUIManagerSubsystem`，
 *   让 `UGameUIPolicy` 能为每个 LocalPlayer 创建并维护 `UPrimaryGameLayout`（RootLayout）。
 */
UCLASS()
class YANGAMECORE_API UYanGameInstance : public UModularGameInstance
{
	GENERATED_BODY()
	
public:
	UYanGameInstance(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	//~Begin UGameInstance Interface
	virtual int32 AddLocalPlayer(ULocalPlayer* NewPlayer, FPlatformUserId UserId) override;
	virtual bool RemoveLocalPlayer(ULocalPlayer* ExistingPlayer) override;
	//~End UGameInstance Interface
};
