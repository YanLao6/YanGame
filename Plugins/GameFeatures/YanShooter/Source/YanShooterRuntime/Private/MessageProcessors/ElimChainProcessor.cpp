// 短时连杀处理器实现。

#include "MessageProcessors/ElimChainProcessor.h"

#include "GameFramework/PlayerState.h"
#include "GameFramework/GameplayMessageSubsystem.h"
#include "Messages/ModularVerbMessage.h"
#include "YanShooterTags.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ElimChainProcessor)

void UElimChainProcessor::StartListening()
{
	UGameplayMessageSubsystem& MessageSubsystem = UGameplayMessageSubsystem::Get(this);
	AddListenerHandle(MessageSubsystem.RegisterListener(YanShooterTags::TAG_Message_Elimination, this, &ThisClass::OnEliminationMessage));
}

void UElimChainProcessor::OnEliminationMessage(FGameplayTag Channel, const FModularVerbMessage& Payload)
{
	// 自杀不计入连杀
	if (Payload.Instigator != Payload.Target)
	{
		if (APlayerState* InstigatorPS = Cast<APlayerState>(Payload.Instigator))
		{
			const double CurrentTime = GetServerTime();

			FPlayerElimChainInfo& History = PlayerChainHistory.FindOrAdd(InstigatorPS);
			const bool bStreakReset = (History.LastEliminationTime == 0.0) || (History.LastEliminationTime + ChainTimeLimit < CurrentTime);

			History.LastEliminationTime = CurrentTime;
			if (bStreakReset)
			{
				History.ChainCounter = 1;
			}
			else
			{
				++History.ChainCounter;

				if (const FGameplayTag* ChainTag = ElimChainTags.Find(History.ChainCounter))
				{
					FModularVerbMessage ElimChainMessage;
					ElimChainMessage.Verb = *ChainTag;
					ElimChainMessage.Instigator = InstigatorPS;
					ElimChainMessage.InstigatorTags = Payload.InstigatorTags;
					ElimChainMessage.ContextTags = Payload.ContextTags;
					ElimChainMessage.Magnitude = History.ChainCounter;

					UGameplayMessageSubsystem& MessageSubsystem = UGameplayMessageSubsystem::Get(this);
					MessageSubsystem.BroadcastMessage(ElimChainMessage.Verb, ElimChainMessage);
				}
			}
		}
	}
}
