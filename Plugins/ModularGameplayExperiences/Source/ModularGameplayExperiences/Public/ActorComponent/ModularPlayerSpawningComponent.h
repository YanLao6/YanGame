// Copyright Chronicler.

#pragma once

#include "Components/GameStateComponent.h"
#include "Player/ModularPlayerStart.h"

#include "ModularPlayerSpawningComponent.generated.h"

class AController;
class APlayerController;
class APlayerState;
class APlayerStart;
class AActor;

UCLASS()
class MODULARGAMEPLAYEXPERIENCES_API UModularPlayerSpawningComponent : public UGameStateComponent
{
	GENERATED_BODY()

public:
	/** 构造玩家生成组件。 */
	explicit UModularPlayerSpawningComponent(const FObjectInitializer& ObjectInitializer);

	UPROPERTY(Transient)
	TArray<TWeakObjectPtr<AModularPlayerStart>> CachedPlayerStarts;

	/** @ingroup UActorComponent：注册 Level/Actor 回调并缓存 PlayerStart。 */
	virtual void InitializeComponent() override;

	/** @ingroup UActorComponent：Tick（当前与 Super 等同，可扩展）。 */
	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

public:
	/** 由 GameMode 代理调用：按当前 Experience 规则选择出生点。 */
	AActor* ChoosePlayerStart(AController* Player);
	/** 由 GameMode 代理调用：判断控制器当前是否允许重生。 */
	bool ControllerCanRestart(AController* Player);
	/** 由 GameMode 代理调用：完成重生后的收尾流程。 */
	void FinishRestartPlayer(AController* NewPlayer, const FRotator& StartRotation);
	friend class AModularExperienceGameMode;

protected:
	// 从候选 PlayerStart 中随机挑选空闲/次优占用点
	APlayerStart* GetFirstRandomUnoccupiedPlayerStart(AController* Controller, const TArray<AModularPlayerStart*>& FoundStartPoints) const;
	
	virtual AActor* OnChoosePlayerStart(AController* Player, TArray<AModularPlayerStart*>& PlayerStarts) { return nullptr; }
	virtual void OnFinishRestartPlayer(AController* Player, const FRotator& StartRotation) { }

	UFUNCTION(BlueprintImplementableEvent, meta=(DisplayName=OnFinishRestartPlayer))
	void K2_OnFinishRestartPlayer(AController* Player, const FRotator& StartRotation);

private:
	void OnLevelAdded(ULevel* InLevel, UWorld* InWorld);
	void HandleOnActorSpawned(AActor* SpawnedActor);

#if WITH_EDITOR
	APlayerStart* FindPlayFromHereStart(AController* Player);
#endif
};
