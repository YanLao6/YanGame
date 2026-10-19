// 助攻处理器实现。

#include "MessageProcessors/AssistProcessor.h"

#include "GameFramework/PlayerState.h"
#include "GameFramework/GameplayMessageSubsystem.h"
#include "Messages/ModularVerbMessage.h"
#include "Messages/VerbMessageHelpers.h"
#include "YanShooterTags.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AssistProcessor)

void UAssistProcessor::StartListening()
{
	UGameplayMessageSubsystem& MessageSubsystem = UGameplayMessageSubsystem::Get(this);
	AddListenerHandle(MessageSubsystem.RegisterListener(YanShooterTags::TAG_Message_Elimination, this, &ThisClass::OnEliminationMessage));
	AddListenerHandle(MessageSubsystem.RegisterListener(YanShooterTags::TAG_Message_Damage, this, &ThisClass::OnDamageMessage));
}

void UAssistProcessor::OnDamageMessage(FGameplayTag Channel, const FModularVerbMessage& Payload)
{
	if (Payload.Instigator != Payload.Target)
	{
		if (APlayerState* InstigatorPS = UVerbMessageHelpers::GetPlayerStateFromObject(Payload.Instigator))
		{
			if (APlayerState* TargetPS = UVerbMessageHelpers::GetPlayerStateFromObject(Payload.Target))
			{
				FPlayerAssistDamageTracking& Damage = DamageHistory.FindOrAdd(TargetPS);
				float& DamageTotalFromTarget = Damage.AccumulatedDamageByPlayer.FindOrAdd(InstigatorPS);
				DamageTotalFromTarget += Payload.Magnitude;
			}
		}
	}
}

void UAssistProcessor::OnEliminationMessage(FGameplayTag Channel, const FModularVerbMessage& Payload)
{
	if (APlayerState* TargetPS = Cast<APlayerState>(Payload.Target))
	{
		// 对目标造成过伤害但不是最后一击的玩家，各计一次助攻
		if (FPlayerAssistDamageTracking* DamageOnTarget = DamageHistory.Find(TargetPS))
		{
			for (const TPair<TObjectPtr<APlayerState>, float>& KVP : DamageOnTarget->AccumulatedDamageByPlayer)
			{
				if (APlayerState* AssistPS = KVP.Key)
				{
					if (AssistPS != Payload.Instigator)
					{
						FModularVerbMessage AssistMessage;
						AssistMessage.Verb = YanShooterTags::TAG_Message_Assist;
						AssistMessage.Instigator = AssistPS;
						AssistMessage.Target = TargetPS;
						AssistMessage.TargetTags = Payload.TargetTags;
						AssistMessage.ContextTags = Payload.ContextTags;
						AssistMessage.Magnitude = KVP.Value;

						UGameplayMessageSubsystem& MessageSubsystem = UGameplayMessageSubsystem::Get(this);
						MessageSubsystem.BroadcastMessage(AssistMessage.Verb, AssistMessage);
					}
				}
			}

			// 目标已被击杀，其伤害记录不再需要
			DamageHistory.Remove(TargetPS);
		}
	}
}
