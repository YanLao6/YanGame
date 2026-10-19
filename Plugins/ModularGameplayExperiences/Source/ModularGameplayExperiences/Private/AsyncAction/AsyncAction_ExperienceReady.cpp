// 等待体验就绪的异步动作实现。

#include "AsyncAction/AsyncAction_ExperienceReady.h"

#include "ActorComponent/ModularExperienceComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "TimerManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AsyncAction_ExperienceReady)

UAsyncAction_ExperienceReady::UAsyncAction_ExperienceReady(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

UAsyncAction_ExperienceReady* UAsyncAction_ExperienceReady::WaitForExperienceReady(UObject* InWorldContextObject)
{
	UAsyncAction_ExperienceReady* Action = nullptr;

	if (UWorld* World = GEngine->GetWorldFromContextObject(InWorldContextObject, EGetWorldErrorMode::LogAndReturnNull))
	{
		Action = NewObject<UAsyncAction_ExperienceReady>();
		Action->WorldPtr = World;
		Action->RegisterWithGameInstance(World);
	}

	return Action;
}

void UAsyncAction_ExperienceReady::Activate()
{
	if (UWorld* World = WorldPtr.Get())
	{
		if (AGameStateBase* GameState = World->GetGameState())
		{
			ListenToExperienceLoading(GameState);
		}
		else
		{
			World->GameStateSetEvent.AddUObject(this, &ThisClass::HandleGameStateSet);
		}
	}
	else
	{
		// 没有世界则永远等不到结果，直接结束
		SetReadyToDestroy();
	}
}

void UAsyncAction_ExperienceReady::HandleGameStateSet(AGameStateBase* GameState)
{
	if (UWorld* World = WorldPtr.Get())
	{
		World->GameStateSetEvent.RemoveAll(this);
	}

	ListenToExperienceLoading(GameState);
}

void UAsyncAction_ExperienceReady::ListenToExperienceLoading(AGameStateBase* GameState)
{
	check(GameState);
	UModularExperienceComponent* ExperienceComponent = GameState->FindComponentByClass<UModularExperienceComponent>();
	check(ExperienceComponent);

	if (ExperienceComponent->IsExperienceLoaded())
	{
		UWorld* World = GameState->GetWorld();
		check(World);

		// 体验恰好已加载完毕，仍延后一帧再通知，避免调用方依赖同步返回
		World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateUObject(this, &ThisClass::BroadcastReady));
	}
	else
	{
		ExperienceComponent->CallOrRegister_OnExperienceLoaded(FOnModularExperienceLoaded::FDelegate::CreateUObject(this, &ThisClass::HandleExperienceLoaded));
	}
}

void UAsyncAction_ExperienceReady::HandleExperienceLoaded(const UModularExperienceDefinition* CurrentExperience)
{
	BroadcastReady();
}

void UAsyncAction_ExperienceReady::BroadcastReady()
{
	OnReady.Broadcast();

	SetReadyToDestroy();
}
