// 短时连杀处理器声明。

#pragma once

#include "CoreMinimal.h"
#include "Messages/ModularGameplayMessageProcessor.h"
#include "ElimChainProcessor.generated.h"

class APlayerState;
struct FGameplayTag;
struct FModularVerbMessage;

/** 单名玩家的短时连杀记录。 */
USTRUCT()
struct FPlayerElimChainInfo
{
	GENERATED_BODY()

	double LastEliminationTime = 0.0;

	int32 ChainCounter = 1;
};

/**
 * 短时连杀处理器。
 *
 * 统计在限定间隔内接连达成的击杀数，与不被击杀即持续累计的连杀不同，
 * 此处只要两次击杀的间隔超过 ChainTimeLimit 即重新计数。
 */
UCLASS(MinimalAPI, Abstract)
class UElimChainProcessor : public UModularGameplayMessageProcessor
{
	GENERATED_BODY()

public:
	//~Begin UModularGameplayMessageProcessor Interface
	virtual void StartListening() override;
	//~End UModularGameplayMessageProcessor Interface

protected:
	/** 两次击杀之间允许的最大间隔，超过即断链，单位为秒。 */
	UPROPERTY(EditDefaultsOnly)
	float ChainTimeLimit = 4.5f;

	/** 连杀数到达某档位时用于广播的 Verb。 */
	UPROPERTY(EditDefaultsOnly)
	TMap<int32, FGameplayTag> ElimChainTags;

private:
	void OnEliminationMessage(FGameplayTag Channel, const FModularVerbMessage& Payload);

	UPROPERTY(Transient)
	TMap<TObjectPtr<APlayerState>, FPlayerElimChainInfo> PlayerChainHistory;
};
