// Copyright Chronicler.

#pragma once

#include "CommonSessionSubsystem.h"
#include "CommonUserSubsystem.h"
#include "ModularExperienceDefinition.h"
#include "ModularGameMode.h"
#include "DataAsset/ModularPawnData.h"
#include "UObject/ObjectKey.h"

#include "ModularExperienceGameMode.generated.h"

#define UE_API MODULARGAMEPLAYEXPERIENCES_API


/**
 * GameMode 侧玩家初始化完成事件。
 *
 * 在玩家或 Bot 加入、以及 Seamless / Non-Seamless Travel 后，玩家完成初始化时广播。
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnModularGameModePlayerInitialized, AGameModeBase*, GameMode, AController*, NewPlayer);

/**
 * Experience 驱动的 GameMode。
 *
 * 将 Pawn 生成、重生与会话流程委托给 Experience 相关组件和数据资产。
 */
UCLASS(MinimalAPI, Config="Game", Meta=(ShortTooltip="本项目使用的 GameMode 基类（Experience 驱动）。"))
class AModularExperienceGameMode : public AModularGameModeBase
{
	GENERATED_BODY()

public:
	/** 构造 ExperienceGameMode。 */
	UE_API explicit AModularExperienceGameMode(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** 根据控制器解析对应的 PawnData。项目层可覆写以插入按玩家的角色决议。 */
	UFUNCTION(BlueprintCallable, Category = "Lyra|Pawn")
	UE_API virtual const UModularPawnData* GetPawnDataForController(const AController* InController) const;

	/**
	 * 下一帧请求重生（Respawn）指定玩家或 Bot。
	 * @param bForceReset 为 true 时本帧先 Reset Controller（放弃当前 Possess 的 Pawn）。
	 */
	UFUNCTION(BlueprintCallable)
	UE_API void RequestPlayerRestartNextFrame(AController* Controller, bool bForceReset = false);

	/**
	 * 指定该 Controller 下一次生成 Pawn 时使用的 Transform，替代出生点决议结果。
	 *
	 * 仅生效一次，消费后自动清除；Controller 失效时条目随之作废。
	 * 用于换装等需要保持原位的重生，避免玩家被拉回 PlayerStart。
	 */
	UFUNCTION(BlueprintCallable, Category = "Spawn")
	UE_API void SetPendingSpawnTransform(AController* Controller, const FTransform& SpawnTransform);

	/** PlayerCanRestart 的通用版：同时适用于真实玩家与 Bot。 */
	UE_API virtual bool ControllerCanRestart(AController* Controller);

	/** 玩家初始化完成时广播，见 FOnModularGameModePlayerInitialized。 */
	FOnModularGameModePlayerInitialized OnGameModePlayerInitialized;

	/** @ingroup AGameModeBase：按 PawnData 解析默认 Pawn Class。 */
	UE_API virtual UClass* GetDefaultPawnClassForController_Implementation(AController* InController) override;

	/** 启动关卡：延一帧再解析 Experience 分配，以便 Startup 设置就绪。 */
	UE_API virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;

	/** 在指定 Transform 生成默认 Pawn，并注入 PawnData（DeferConstruction + FinishSpawning）。 */
	UE_API virtual APawn* SpawnDefaultPawnAtTransform_Implementation(AController* NewPlayer, const FTransform& SpawnTransform) override;

	/**
	 * 禁用引擎默认的 PlayerStart 出生逻辑；出生由 UModularPlayerSpawningComponent 管理。
	 * @return 恒为 false
	 */
	UE_API virtual bool ShouldSpawnAtStartSpot(AController* Player) override;

	/** Experience 已加载时才启动新玩家；否则由 OnExperienceLoaded 统一 Restart。 */
	UE_API virtual void HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer) override;

	/** 委托给 UModularPlayerSpawningComponent 选择 PlayerStart。 */
	UE_API virtual AActor* ChoosePlayerStart_Implementation(AController* Player) override;

	/** 委托给 PlayerSpawning 完成重生收尾，并调用基类流程。 */
	UE_API virtual void FinishRestartPlayer(AController* NewPlayer, const FRotator& StartRotation) override;

	/** 转发至 ControllerCanRestart。 */
	UE_API virtual bool PlayerCanRestart_Implementation(APlayerController* Player) override;

	/** 注册 Experience 加载完成回调。 */
	UE_API virtual void InitGameState() override;

	/**
	 * PostLogin 前不更新 PlayerStart：Experience 未加载前无法正确选点；分队等系统也尚未完成。
	 * @return 恒为 true（占位通过）
	 */
	UE_API virtual bool UpdatePlayerStartSpot(AController* Player, const FString& Portal, FString& OutErrorMessage) override;

	/** 基类初始化后广播 OnGameModePlayerInitialized。 */
	UE_API virtual void GenericPlayerInitialization(AController* NewPlayer) override;

	/** 生成失败时在条件允许下排队下一帧再次 Restart。 */
	UE_API virtual void FailedToRestartPlayer(AController* NewPlayer) override;

protected:
	// 待用的一次性生成 Transform，键为 Controller。
	TMap<FObjectKey, FTransform> PendingSpawnTransforms;

	/** Dedicated Server 在线登录完成：按结果选择 Online / LAN  Hosting。 */
	UFUNCTION()
	UE_API void OnUserInitializedForDedicatedServer(const UCommonUserInfo* UserInfo, bool bSuccess, FText Error, ECommonUserPrivilege RequestedPrivilege, ECommonUserOnlineContext OnlineContext);

	UE_API void OnExperienceLoaded(const UModularExperienceDefinition* CurrentExperience);
	UE_API bool IsExperienceLoaded() const;
	UE_API void OnMatchAssignmentGiven(const FPrimaryAssetId& ExperienceId, const FString& ExperienceIdSource);
	UE_API void HandleMatchAssignmentIfNotExpectingOne();

	//@todo 将 Dedicated Server 流程封装进可配置的 Hosting 方案。
	UE_API bool TryDedicatedServerLogin();
	UE_API void HostDedicatedServerMatch(ECommonSessionOnlineMode OnlineMode);
};

#undef UE_API
