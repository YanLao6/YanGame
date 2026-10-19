// 连续击杀处理器实现。

#include "MessageProcessors/ElimStreakProcessor.h"

#include "GameFramework/PlayerState.h"
#include "GameFramework/GameplayMessageSubsystem.h"
#include "Messages/ModularVerbMessage.h"
#include "YanShooterTags.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ElimStreakProcessor)

void UElimStreakProcessor::StartListening()
{
	UGameplayMessageSubsystem& MessageSubsystem = UGameplayMessageSubsystem::Get(this);
	AddListenerHandle(MessageSubsystem.RegisterListener(YanShooterTags::TAG_Message_Elimination, this, &ThisClass::OnEliminationMessage));
}

void UElimStreakProcessor::OnEliminationMessage(FGameplayTag Channel, const FModularVerbMessage& Payload)
{
	// 自杀不计入连杀
	if (Payload.Instigator != Payload.Target)
	{
		if (APlayerState* InstigatorPS = Cast<APlayerState>(Payload.Instigator))
		{
			int32& StreakCount = PlayerStreakHistory.FindOrAdd(InstigatorPS);
			StreakCount++;

			if (const FGameplayTag* StreakTag = ElimStreakTags.Find(StreakCount))
			{
				FModularVerbMessage ElimStreakMessage;
				ElimStreakMessage.Verb = *StreakTag;
				ElimStreakMessage.Instigator = InstigatorPS;
				ElimStreakMessage.InstigatorTags = Payload.InstigatorTags;
				ElimStreakMessage.ContextTags = Payload.ContextTags;
				ElimStreakMessage.Magnitude = StreakCount;

				UGameplayMessageSubsystem& MessageSubsystem = UGameplayMessageSubsystem::Get(this);
				MessageSubsystem.BroadcastMessage(ElimStreakMessage.Verb, ElimStreakMessage);
			}
		}
	}

	// 被击杀者的连杀中断
	if (APlayerState* TargetPS = Cast<APlayerState>(Payload.Target))
	{
		PlayerStreakHistory.Remove(TargetPS);
	}
}
