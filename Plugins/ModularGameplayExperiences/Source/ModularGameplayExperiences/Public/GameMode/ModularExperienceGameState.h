// Copyright Chronicler.

#pragma once

#include "ModularGameState.h"
#include "ActorComponent/ModularExperienceComponent.h"
#include "Messages/ModularVerbMessage.h"

#include "ModularExperienceGameState.generated.h"

#define UE_API MODULARGAMEPLAYEXPERIENCES_API

/**
 * Experience 对应的 GameState。
 *
 * 承载 ExperienceComponent，并提供面向客户端的消息广播能力。
 */
UCLASS(MinimalAPI, Config = Game)
class AModularExperienceGameState : public AModularGameStateBase
{
	GENERATED_BODY()

public:
	/** 构造 ExperienceGameState。 */
	UE_API AModularExperienceGameState(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	//~AActor interface
	UE_API virtual void PreInitializeComponents() override;
	UE_API virtual void PostInitializeComponents() override;
	UE_API virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	UE_API virtual void Tick(float DeltaSeconds) override;
	//~End of AActor interface

	//~AGameStateBase interface
	UE_API virtual void AddPlayerState(APlayerState* PlayerState) override;
	UE_API virtual void RemovePlayerState(APlayerState* PlayerState) override;
	UE_API virtual void SeamlessTravelTransitionCheckpoint(bool bToTransitionMap) override;
	//~End of AGameStateBase interface

	/**
	 * Unreliable NetMulticast：向客户端广播 VerbMessage（可丢）。
	 * 适用于淘汰提示、进房提示等可容忍丢失的通知。
	 */
	UFUNCTION(NetMulticast, Unreliable, BlueprintCallable, Category = "GameState")
	UE_API void MulticastMessageToClients(const FModularVerbMessage Message);

	/**
	 * Reliable NetMulticast：向客户端保证送达的 VerbMessage。
	 * 仅用于不可丢失的客户端通知。
	 */
	UFUNCTION(NetMulticast, Reliable, BlueprintCallable, Category = "GameState")
	UE_API void MulticastReliableMessageToClients(const FModularVerbMessage Message);

	/** 获取服务器端 FPS（Replicated 到客户端）。 */
	UE_API float GetServerFPS() const;

	/** 标记本地用于录制 Replay 的 PlayerState（通常仅录制端调用）。 */
	UE_API void SetRecorderPlayerState(APlayerState* NewPlayerState);

	/** 返回录制 Replay 时关联的 PlayerState（若有效）。 */
	UE_API APlayerState* GetRecorderPlayerState() const;

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
	UE_API void OnRep_RecorderPlayerState();

};

#undef UE_API
