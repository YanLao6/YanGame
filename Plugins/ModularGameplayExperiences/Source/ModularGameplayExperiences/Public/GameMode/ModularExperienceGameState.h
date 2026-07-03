// Copyright Chronicler.

#pragma once

#include "ModularGameState.h"
#include "ActorComponent/ModularExperienceComponent.h"
#include "Messages/ModularVerbMessage.h"

#include "ModularExperienceGameState.generated.h"

/**
 * Experience 对应的 GameState。
 *
 * 承载 ExperienceComponent，并提供面向客户端的消息广播能力。
 */
UCLASS(Config = Game)
class MODULARGAMEPLAYEXPERIENCES_API AModularExperienceGameState : public AModularGameStateBase
{
	GENERATED_BODY()

public:
	/** 构造 ExperienceGameState。 */
	AModularExperienceGameState(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	//~AActor interface
	virtual void PreInitializeComponents() override;
	virtual void PostInitializeComponents() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaSeconds) override;
	//~End of AActor interface

	//~AGameStateBase interface
	virtual void AddPlayerState(APlayerState* PlayerState) override;
	virtual void RemovePlayerState(APlayerState* PlayerState) override;
	virtual void SeamlessTravelTransitionCheckpoint(bool bToTransitionMap) override;
	//~End of AGameStateBase interface

	/**
	 * Unreliable NetMulticast：向客户端广播 VerbMessage（可丢）。
	 * 适用于淘汰提示、进房提示等可容忍丢失的通知。
	 */
	UFUNCTION(NetMulticast, Unreliable, BlueprintCallable, Category = "GameState")
	void MulticastMessageToClients(const FModularVerbMessage Message);

	/**
	 * Reliable NetMulticast：向客户端保证送达的 VerbMessage。
	 * 仅用于不可丢失的客户端通知。
	 */
	UFUNCTION(NetMulticast, Reliable, BlueprintCallable, Category = "GameState")
	void MulticastReliableMessageToClients(const FModularVerbMessage Message);

	/** 获取服务器端 FPS（Replicated 到客户端）。 */
	float GetServerFPS() const;

	/** 标记本地用于录制 Replay 的 PlayerState（通常仅录制端调用）。 */
	void SetRecorderPlayerState(APlayerState* NewPlayerState);

	/** 返回录制 Replay 时关联的 PlayerState（若有效）。 */
	APlayerState* GetRecorderPlayerState() const;

	/** Replay 录制 PlayerState 变化时广播。 */
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnRecorderPlayerStateChanged, APlayerState*);
	FOnRecorderPlayerStateChanged OnRecorderPlayerStateChangedEvent;

private:
	// 当前 Experience 加载与管理（组件子对象）
	UPROPERTY()
	TObjectPtr<UModularExperienceComponent> ModularExperienceComponent;

protected:
	UPROPERTY(Replicated)
	float ServerFPS;

	// 录制 Replay 时的观察目标 PlayerState；COND_ReplayOnly，正常对局不复制
	UPROPERTY(Transient, ReplicatedUsing = OnRep_RecorderPlayerState)
	TObjectPtr<APlayerState> RecorderPlayerState;

	UFUNCTION()
	void OnRep_RecorderPlayerState();

};
