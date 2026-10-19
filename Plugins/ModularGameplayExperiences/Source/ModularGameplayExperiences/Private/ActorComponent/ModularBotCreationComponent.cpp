// Bot 创建组件实现。

#include "ActorComponent/ModularBotCreationComponent.h"

#include "AIController.h"
#include "ActorComponent/ModularExperienceComponent.h"
#include "ActorComponent/ModularPawnComponent.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerState.h"
#include "GameMode/ModularExperienceGameMode.h"
#include "Kismet/GameplayStatics.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ModularBotCreationComponent)

UModularBotCreationComponent::UModularBotCreationComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UModularBotCreationComponent::BeginPlay()
{
	Super::BeginPlay();

	// Bot 需要 PawnData 与 AbilitySet 就位后才能入场，因此等 Experience 加载完成再生成
	AGameStateBase* GameState = GetGameStateChecked<AGameStateBase>();
	UModularExperienceComponent* ExperienceComponent = GameState->FindComponentByClass<UModularExperienceComponent>();
	check(ExperienceComponent);
	ExperienceComponent->CallOrRegister_OnExperienceLoaded_LowPriority(
		FOnModularExperienceLoaded::FDelegate::CreateUObject(this, &ThisClass::OnExperienceLoaded));
}

void UModularBotCreationComponent::OnExperienceLoaded(const UModularExperienceDefinition* Experience)
{
#if WITH_SERVER_CODE
	if (HasAuthority())
	{
		ServerCreateBots();
	}
#endif
}

#if WITH_SERVER_CODE

void UModularBotCreationComponent::ServerCreateBots_Implementation()
{
	if (BotControllerClass == nullptr)
	{
		return;
	}

	RemainingBotNames = RandomBotNames;

	int32 EffectiveBotCount = NumBotsToCreate;

	// 启动 URL 可覆盖数量，便于压测与联机调试
	if (const AGameModeBase* GameModeBase = GetGameMode<AGameModeBase>())
	{
		EffectiveBotCount = UGameplayStatics::GetIntOption(GameModeBase->OptionsString, TEXT("NumBots"), EffectiveBotCount);
	}

	for (int32 Count = 0; Count < EffectiveBotCount; ++Count)
	{
		SpawnOneBot();
	}
}

FString UModularBotCreationComponent::CreateBotName(int32 PlayerIndex)
{
	FString Result;
	if (RemainingBotNames.Num() > 0)
	{
		const int32 NameIndex = FMath::RandRange(0, RemainingBotNames.Num() - 1);
		Result = RemainingBotNames[NameIndex];
		RemainingBotNames.RemoveAtSwap(NameIndex);
	}
	else
	{
		PlayerIndex = FMath::RandRange(260, 360);
		Result = FString::Printf(TEXT("Bot %d"), PlayerIndex);
	}
	return Result;
}

void UModularBotCreationComponent::SpawnOneBot()
{
	FActorSpawnParameters SpawnInfo;
	SpawnInfo.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	SpawnInfo.OverrideLevel = GetComponentLevel();
	SpawnInfo.ObjectFlags |= RF_Transient;
	AAIController* NewController = GetWorld()->SpawnActor<AAIController>(BotControllerClass, FVector::ZeroVector, FRotator::ZeroRotator, SpawnInfo);

	if (NewController != nullptr)
	{
		AModularExperienceGameMode* GameMode = GetGameMode<AModularExperienceGameMode>();
		check(GameMode);

		if (NewController->PlayerState != nullptr)
		{
			NewController->PlayerState->SetPlayerName(CreateBotName(NewController->PlayerState->GetPlayerId()));
		}

		GameMode->GenericPlayerInitialization(NewController);
		GameMode->RestartPlayer(NewController);

		if (NewController->GetPawn() != nullptr)
		{
			if (UModularPawnComponent* PawnExtComponent = NewController->GetPawn()->FindComponentByClass<UModularPawnComponent>())
			{
				PawnExtComponent->CheckDefaultInitialization();
			}
		}

		SpawnedBotList.Add(NewController);
	}
}

void UModularBotCreationComponent::RemoveOneBot()
{
	if (SpawnedBotList.Num() > 0)
	{
		// Bot 之间没有差异，随机移除一个即可
		const int32 BotToRemoveIndex = FMath::RandRange(0, SpawnedBotList.Num() - 1);

		AAIController* BotToRemove = SpawnedBotList[BotToRemoveIndex];
		SpawnedBotList.RemoveAtSwap(BotToRemoveIndex);

		if (BotToRemove)
		{
			if (APawn* ControlledPawn = BotToRemove->GetPawn())
			{
				ControlledPawn->Destroy();
			}

			// 销毁控制器会触发 Logout 等常规流程
			BotToRemove->Destroy();
		}
	}
}

#else // !WITH_SERVER_CODE

void UModularBotCreationComponent::ServerCreateBots_Implementation()
{
	ensureMsgf(0, TEXT("Bot 相关功能不存在于纯客户端目标中。"));
}

void UModularBotCreationComponent::SpawnOneBot()
{
	ensureMsgf(0, TEXT("Bot 相关功能不存在于纯客户端目标中。"));
}

void UModularBotCreationComponent::RemoveOneBot()
{
	ensureMsgf(0, TEXT("Bot 相关功能不存在于纯客户端目标中。"));
}

#endif
