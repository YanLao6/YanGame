#include "HeroAssignmentComponent.h"

#include "HeroAssignmentSystem.h"
#include "HeroCatalog.h"
#include "HeroSelectionSubsystem.h"
#include "ActorComponent/ModularExperienceComponent.h"
#include "DataAsset/ModularPawnData.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
#include "GameMode/ModularExperienceGameMode.h"
#include "Player/ModularExperiencePlayerState.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HeroAssignmentComponent)

UHeroAssignmentComponent::UHeroAssignmentComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UHeroAssignmentComponent::BeginPlay()
{
	Super::BeginPlay();

	// 组件由 GameFeature 注入，BeginPlay 时玩家可能尚未登录，分配推迟到 Experience 就绪。
	// 采用高优先级：先于 PlayerState 的同名回调执行，使其首次写入 PawnData 即为最终角色。
	const AGameStateBase* GameState = GetGameStateChecked<AGameStateBase>();
	UModularExperienceComponent* ExperienceComponent = GameState->FindComponentByClass<UModularExperienceComponent>();
	check(ExperienceComponent);

	ExperienceComponent->CallOrRegister_OnExperienceLoaded_HighPriority(
		FOnModularExperienceLoaded::FDelegate::CreateUObject(this, &ThisClass::OnExperienceLoaded));
}

void UHeroAssignmentComponent::OnExperienceLoaded(const UModularExperienceDefinition* Experience)
{
	const AActor* Owner = GetOwner();
	if (!Owner || !Owner->HasAuthority())
	{
		return;
	}

	const AGameStateBase* GameState = GetGameStateChecked<AGameStateBase>();
	for (APlayerState* PlayerState : GameState->PlayerArray)
	{
		if (PlayerState)
		{
			AssignHeroForController(PlayerState->GetOwningController());
		}
	}

	if (AModularExperienceGameMode* GameMode = GetGameMode<AModularExperienceGameMode>())
	{
		GameMode->OnGameModePlayerInitialized.AddDynamic(this, &ThisClass::OnPlayerInitialized);
	}
}

const UModularPawnData* UHeroAssignmentComponent::ResolveHeroForPlayer(const AController* Controller) const
{
	if (!Controller)
	{
		return nullptr;
	}

	if (const UModularPawnData* const* Found = AssignedHeroes.Find(FObjectKey(Controller)))
	{
		return *Found;
	}

	return nullptr;
}

const UModularPawnData* UHeroAssignmentComponent::FindSelectableHero(const FPrimaryAssetId& HeroId) const
{
	return HeroCatalog ? HeroCatalog->FindPawnData(HeroId) : nullptr;
}

bool UHeroAssignmentComponent::ChangeHeroForController(AController* Controller, FPrimaryAssetId HeroId)
{
	const AActor* Owner = GetOwner();
	if (!Controller || !Owner || !Owner->HasAuthority())
	{
		return false;
	}

	const UModularPawnData* NewHero = FindSelectableHero(HeroId);
	if (!NewHero)
	{
		UE_LOG(LogHeroAssignment,
		       Warning,
		       TEXT("玩家 [%s] 请求更换的角色 [%s] 不在本局目录内，已拒绝。"),
		       *GetNameSafe(Controller->PlayerState),
		       *HeroId.ToString());
		return false;
	}

	AModularExperiencePlayerState* PlayerState = Controller->GetPlayerState<AModularExperiencePlayerState>();
	if (!PlayerState)
	{
		return false;
	}

	if (PlayerState->GetPawnData<UModularPawnData>() == NewHero)
	{
		return false;
	}

	// 先改写登记，使旧角色立即可被其他玩家占用
	AssignedHeroes.Add(FObjectKey(Controller), NewHero);

	PlayerState->ChangePawnData(NewHero);

	if (AModularExperienceGameMode* GameMode = GetGameMode<AModularExperienceGameMode>())
	{
		// 重生发生在下一帧，此刻旧 Pawn 仍在，位姿需在这里取得。
		if (bRestartInPlace)
		{
			if (const APawn* CurrentPawn = Controller->GetPawn())
			{
				GameMode->SetPendingSpawnTransform(Controller, CurrentPawn->GetActorTransform());
			}
		}

		GameMode->RequestPlayerRestartNextFrame(Controller);
	}

	UE_LOG(LogHeroAssignment,
	       Log,
	       TEXT("玩家 [%s] 已更换角色为 [%s]。"),
	       *GetNameSafe(Controller->PlayerState),
	       *GetNameSafe(NewHero));

	return true;
}

bool UHeroAssignmentComponent::IsManagingHeroAssignment() const
{
	return HeroCatalog && !HeroCatalog->IsEmpty();
}

void UHeroAssignmentComponent::AssignHeroForController(AController* Controller)
{
	if (!Controller || !IsManagingHeroAssignment())
	{
		return;
	}

	const FObjectKey ControllerKey(Controller);
	if (AssignedHeroes.Contains(ControllerKey))
	{
		return;
	}

	const UModularPawnData* Hero = nullptr;

	// 优先使用玩家在大厅登记的选择
	if (const APlayerState* PlayerState = Controller->PlayerState)
	{
		if (const UGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr)
		{
			if (const UHeroSelectionSubsystem* Selection = GameInstance->GetSubsystem<UHeroSelectionSubsystem>())
			{
				const FPrimaryAssetId SelectedHeroId = Selection->GetSelectedHero(PlayerState->GetUniqueId());
				Hero                                 = FindSelectableHero(SelectedHeroId);

				// 选择已登记却不在目录内，说明大厅与本局引用了不同的目录资产
				if (SelectedHeroId.IsValid() && !Hero)
				{
					UE_LOG(LogHeroAssignment,
					       Warning,
					       TEXT("玩家 [%s] 选择的角色 [%s] 不在本局目录内，将回退到兜底分配。"),
					       *PlayerState->GetPlayerName(),
					       *SelectedHeroId.ToString());
				}
			}
		}
	}

	if (!Hero)
	{
		Hero = ChooseUnclaimedHero(Controller);
		UE_LOG(LogHeroAssignment,
		       Log,
		       TEXT("玩家 [%s] 无可用选择，兜底分配角色 [%s]。"),
		       *GetNameSafe(Controller->PlayerState),
		       *GetNameSafe(Hero));
	}

	if (Hero)
	{
		AssignedHeroes.Add(ControllerKey, Hero);
	}
}

const UModularPawnData* UHeroAssignmentComponent::ChooseUnclaimedHero(const AController* Controller) const
{
	if (!HeroCatalog)
	{
		return nullptr;
	}

	const FObjectKey              RequestingKey(Controller);
	TSet<const UModularPawnData*> ClaimedHeroes;
	for (const TPair<FObjectKey, const UModularPawnData*>& Assignment : AssignedHeroes)
	{
		if (Assignment.Key != RequestingKey && Assignment.Value)
		{
			ClaimedHeroes.Add(Assignment.Value);
		}
	}

	const UModularPawnData* FirstValidHero = nullptr;
	for (const FHeroEntry& Entry : HeroCatalog->Heroes)
	{
		if (!Entry.PawnData)
		{
			continue;
		}

		if (!ClaimedHeroes.Contains(Entry.PawnData.Get()))
		{
			return Entry.PawnData.Get();
		}

		if (!FirstValidHero)
		{
			FirstValidHero = Entry.PawnData.Get();
		}
	}

	// 目录条目少于玩家数时允许重复分配
	return FirstValidHero;
}

void UHeroAssignmentComponent::OnPlayerInitialized(AGameModeBase* GameMode, AController* NewPlayer)
{
	AssignHeroForController(NewPlayer);

	if (!NewPlayer)
	{
		return;
	}

	// Experience 早于玩家登录完成时，PlayerState 的首次 PawnData 查询发生在玩家身份就绪之前，
	// 那一轮不会写入任何值；此处补写，使后加入的玩家同样拿到分配结果。
	AModularExperiencePlayerState* PlayerState = NewPlayer->GetPlayerState<AModularExperiencePlayerState>();
	if (!PlayerState || PlayerState->GetPawnData<UModularPawnData>())
	{
		return;
	}

	if (const UModularPawnData* AssignedHero = ResolveHeroForPlayer(NewPlayer))
	{
		PlayerState->SetPawnData(AssignedHero);
	}
}
