// 连续击杀处理器声明。

#pragma once

#include "CoreMinimal.h"
#include "Messages/ModularGameplayMessageProcessor.h"
#include "ElimStreakProcessor.generated.h"

class APlayerState;
struct FGameplayTag;
struct FModularVerbMessage;

/**
 * 连续击杀处理器。
 *
 * 统计每名玩家在未被击杀期间的连续击杀数，达到配置的档位时以对应 Verb 广播一条新消息。
 * 玩家被击杀时其连杀计数清空。
 */
UCLASS(MinimalAPI, Abstract)
class UElimStreakProcessor : public UModularGameplayMessageProcessor
{
	GENERATED_BODY()

public:
	//~Begin UModularGameplayMessageProcessor Interface
	virtual void StartListening() override;
	//~End UModularGameplayMessageProcessor Interface

protected:
	/** 连杀数到达某档位时用于广播的 Verb。 */
	UPROPERTY(EditDefaultsOnly)
	TMap<int32, FGameplayTag> ElimStreakTags;

private:
	void OnEliminationMessage(FGameplayTag Channel, const FModularVerbMessage& Payload);

	UPROPERTY(Transient)
	TMap<TObjectPtr<APlayerState>, int32> PlayerStreakHistory;
};
