// 助攻处理器声明。

#pragma once

#include "CoreMinimal.h"
#include "Messages/ModularGameplayMessageProcessor.h"
#include "AssistProcessor.generated.h"

class APlayerState;
struct FGameplayTag;
struct FModularVerbMessage;

/** 某名玩家身上累计承受的伤害，按施加者分列。 */
USTRUCT()
struct FPlayerAssistDamageTracking
{
	GENERATED_BODY()

	UPROPERTY(Transient)
	TMap<TObjectPtr<APlayerState>, float> AccumulatedDamageByPlayer;
};

/**
 * 助攻处理器。
 *
 * 累计每名玩家受到的伤害来源，在其被击杀时，向除最后一击之外的所有伤害来源广播助攻。
 */
UCLASS(MinimalAPI)
class UAssistProcessor : public UModularGameplayMessageProcessor
{
	GENERATED_BODY()

public:
	//~Begin UModularGameplayMessageProcessor Interface
	virtual void StartListening() override;
	//~End UModularGameplayMessageProcessor Interface

private:
	void OnDamageMessage(FGameplayTag Channel, const FModularVerbMessage& Payload);
	void OnEliminationMessage(FGameplayTag Channel, const FModularVerbMessage& Payload);

	UPROPERTY(Transient)
	TMap<TObjectPtr<APlayerState>, FPlayerAssistDamageTracking> DamageHistory;
};
