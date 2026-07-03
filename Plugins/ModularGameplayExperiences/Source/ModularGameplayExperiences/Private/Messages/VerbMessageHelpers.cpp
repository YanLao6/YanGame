// Copyright Chronicler.


#include "Messages/VerbMessageHelpers.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "GameplayEffectTypes.h"
#include "Messages/ModularVerbMessage.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(VerbMessageHelpers)

//////////////////////////////////////////////////////////////////////
// FModularVerbMessage

FString FModularVerbMessage::ToString() const
{
	FString HumanReadableMessage;
	FModularVerbMessage::StaticStruct()->ExportText(/*out*/ HumanReadableMessage, this, /*Defaults=*/ nullptr, /*OwnerObject=*/ nullptr, PPF_None, /*ExportRootScope=*/ nullptr);
	return HumanReadableMessage;
}

//////////////////////////////////////////////////////////////////////
// UVerbMessageHelpers

APlayerState* UVerbMessageHelpers::GetPlayerStateFromObject(UObject* Object)
{
	if (APlayerController* PC = Cast<APlayerController>(Object))
	{
		return PC->PlayerState;
	}

	if (APlayerState* TargetPS = Cast<APlayerState>(Object))
	{
		return TargetPS;
	}

	if (APawn* TargetPawn = Cast<APawn>(Object))
	{
		if (APlayerState* TargetPS = TargetPawn->GetPlayerState())
		{
			return TargetPS;
		}
	}
	return nullptr;
}

APlayerController* UVerbMessageHelpers::GetPlayerControllerFromObject(UObject* Object)
{
	if (APlayerController* PC = Cast<APlayerController>(Object))
	{
		return PC;
	}

	if (APlayerState* TargetPS = Cast<APlayerState>(Object))
	{
		return TargetPS->GetPlayerController();
	}

	if (APawn* TargetPawn = Cast<APawn>(Object))
	{
		return Cast<APlayerController>(TargetPawn->GetController());
	}

	return nullptr;
}

APawn* UVerbMessageHelpers::GetPlayerPawnFromObject(UObject* Object)
{
	if (APlayerController* PC = Cast<APlayerController>(Object))
	{
		return PC->GetPawn();
	}

	if (APlayerState* TargetPS = Cast<APlayerState>(Object))
	{
		return TargetPS->GetPawn();
	}

	if (APawn* TargetPawn = Cast<APawn>(Object))
	{
		return TargetPawn;
	}

	return nullptr;
}

UAbilitySystemComponent* UVerbMessageHelpers::GetAbilitySystemComponentFromObject(UObject* Object)
{
	if (APlayerState* TargetPS = GetPlayerStateFromObject(Object))
	{
		if (UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(TargetPS, /*LookForComponent=*/true))
		{
			return ASC;
		}
	}

	if (APawn* TargetPawn = GetPlayerPawnFromObject(Object))
	{
		if (UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(TargetPawn, /*LookForComponent=*/true))
		{
			return ASC;
		}
	}

	return UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Cast<AActor>(Object), /*LookForComponent=*/true);
}

FGameplayCueParameters UVerbMessageHelpers::VerbMessageToCueParameters(const FModularVerbMessage& Message)
{
	FGameplayCueParameters Result;

	Result.OriginalTag = Message.Verb;
	Result.Instigator = Cast<AActor>(Message.Instigator);
	Result.EffectCauser = Cast<AActor>(Message.Target);
	Result.AggregatedSourceTags = Message.InstigatorTags;
	Result.AggregatedTargetTags = Message.TargetTags;
	//@TODO 映射 Message.ContextTags
	Result.RawMagnitude = Message.Magnitude;

	return Result;
}

FModularVerbMessage UVerbMessageHelpers::CueParametersToVerbMessage(const FGameplayCueParameters& Params)
{
	FModularVerbMessage Result;

	Result.Verb = Params.OriginalTag;
	Result.Instigator = Params.Instigator.Get();
	Result.Target = Params.EffectCauser.Get();
	Result.InstigatorTags = Params.AggregatedSourceTags;
	Result.TargetTags = Params.AggregatedTargetTags;
	//@TODO 反向填充 Result.ContextTags
	Result.Magnitude = Params.RawMagnitude;

	return Result;
}
