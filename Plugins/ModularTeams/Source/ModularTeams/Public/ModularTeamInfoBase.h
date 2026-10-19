// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GameFramework/Info.h"
#include "GameplayTagStack.h"

#include "ModularTeamInfoBase.generated.h"

#define UE_API MODULARTEAMS_API

namespace EEndPlayReason { enum Type : int; }

class UModularTeamCreationComponent;
class UModularTeamSubsystem;

/**
 * 队伍信息 Actor 基类。
 *
 * 每支队伍由服务端生成，携带队伍编号与队伍级 Tag 栈，复制到所有客户端后向队伍子系统注册。
 */
UCLASS(MinimalAPI, Abstract)
class AModularTeamInfoBase : public AInfo
{
	GENERATED_BODY()

public:
	UE_API explicit AModularTeamInfoBase(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	int32 GetTeamId() const { return TeamId; }

	//~Begin AActor Interface
	UE_API virtual void BeginPlay() override;
	UE_API virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	UE_API virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	//~End AActor Interface

protected:
	UE_API virtual void RegisterWithTeamSubsystem(UModularTeamSubsystem* Subsystem);

	// 队伍编号与子系统均就绪时才注册，服务端与客户端到达该状态的顺序不同
	UE_API void TryRegisterWithTeamSubsystem();

private:
	void SetTeamId(int32 NewTeamId);

	UFUNCTION()
	void OnRep_TeamId();

public:
	friend UModularTeamCreationComponent;

	UPROPERTY(Replicated)
	FGameplayTagStackContainer TeamTags;

private:
	UPROPERTY(ReplicatedUsing = OnRep_TeamId)
	int32 TeamId;
};

#undef UE_API
