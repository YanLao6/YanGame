// Copyright Chronicler.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameFramework/Actor.h"
#include "GameFramework/GameplayMessageSubsystem.h"

#include "ModularGameplayMessageProcessor.generated.h"

namespace EEndPlayReason { enum Type : int; }

class UObject;

/**
 * GameplayMessage 处理器基类：监听其它消息并可能二次广播（连招、组合技等）。
 *
 * 注意：处理器通常在 Server 上全局生成一份（非每玩家），需自行按 Owner/Role 过滤受众。
 */

UCLASS(BlueprintType, Blueprintable, meta=(BlueprintSpawnableComponent))
class MODULARGAMEPLAYEXPERIENCES_API UModularGameplayMessageProcessor : public UActorComponent
{
	GENERATED_BODY()

public:
	//~UActorComponent interface
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	//~End of UActorComponent interface

	/** 开始注册 UGameplayMessageSubsystem 监听（子类实现）。 */
	virtual void StartListening();
	/** 停止监听（子类可覆盖）；EndPlay 会清理 Handle。 */
	virtual void StopListening();

protected:
	void AddListenerHandle(FGameplayMessageListenerHandle&& Handle);
	double GetServerTime() const;

private:
	TArray<FGameplayMessageListenerHandle> ListenerHandles;
};
