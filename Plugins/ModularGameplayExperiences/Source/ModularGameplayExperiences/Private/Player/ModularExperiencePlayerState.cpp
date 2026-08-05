// Copyright Chronicler.


#include "Player/ModularExperiencePlayerState.h"

#include "Engine/World.h"
#include "ModularGameplayExperiencesLogs.h"
#include "ActorComponent/ModularExperienceComponent.h"
#include "ActorComponent/ModularPawnComponent.h"
#include "Components/GameFrameworkComponentManager.h"
#include "GameMode/ModularExperienceGameMode.h"
#include "Net/UnrealNetwork.h"
#include "Net/Core/PushModel/PushModel.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ModularExperiencePlayerState)


AModularExperiencePlayerState::AModularExperiencePlayerState(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	  , MyPlayerConnectionType(EModularPlayerConnectionType::Player)
{}

void AModularExperiencePlayerState::SetPawnData(const UModularPawnData* InPawnData)
{
	check(InPawnData);

	if (GetLocalRole() != ROLE_Authority)
	{
		return;
	}

	if (PawnData)
	{
		UE_LOG(LogModularGameplayExperiences,
		       Error,
		       TEXT("Trying to set PawnData [%s] on player state [%s] that already has valid PawnData [%s]."),
		       *GetNameSafe(InPawnData),
		       *GetNameSafe(this),
		       *GetNameSafe(PawnData));
		return;
	}

	MARK_PROPERTY_DIRTY_FROM_NAME(ThisClass, PawnData, this);
	PawnData = InPawnData;

	ApplyPawnDataFragments();

	ForceNetUpdate();
}

void AModularExperiencePlayerState::ChangePawnData(const UModularPawnData* InPawnData)
{
	check(InPawnData);

	if (GetLocalRole() != ROLE_Authority)
	{
		return;
	}

	if (PawnData == InPawnData)
	{
		return;
	}

	RevokePawnDataFragments();

	MARK_PROPERTY_DIRTY_FROM_NAME(ThisClass, PawnData, this);
	PawnData = InPawnData;

	ApplyPawnDataFragments();

	ForceNetUpdate();
}

FPawnDataFragmentContext AModularExperiencePlayerState::MakeFragmentContext(AActor* TargetActor) const
{
	FPawnDataFragmentContext Context;
	Context.TargetActor = TargetActor;
	Context.Pawn        = GetPawn();
	Context.Controller  = GetOwningController();

	return Context;
}

void AModularExperiencePlayerState::ApplyPawnDataFragments()
{
	if (AppliedPawnData)
	{
		// 上一份注入尚未撤销，继续应用会产生无法回收的残留。
		UE_LOG(LogModularGameplayExperiences,
		       Error,
		       TEXT("PlayerState [%s] 在未撤销 PawnData [%s] 的情况下重复应用注入。"),
		       *GetNameSafe(this),
		       *GetNameSafe(AppliedPawnData));
		return;
	}

	if (!PawnData)
	{
		return;
	}

	const bool bHasAuthority = HasAuthority();

	PawnData->ApplyFragments(EPawnDataFragmentScope::PlayerState, MakeFragmentContext(this), bHasAuthority, FragmentStates);

	// Controller 作用域同样跨重生存活，由 PlayerState 一并托管其生命周期。
	if (AController* OwningController = GetOwningController())
	{
		PawnData->ApplyFragments(EPawnDataFragmentScope::Controller, MakeFragmentContext(OwningController), bHasAuthority, FragmentStates);
	}
	else if (bHasAuthority && PawnData->HasFragmentInScope(EPawnDataFragmentScope::Controller))
	{
		// 仅在 Authority 上告警：客户端持有的远端玩家 PlayerState 本就没有 Controller。
		UE_LOG(LogModularGameplayExperiences,
		       Warning,
		       TEXT("PlayerState [%s] 应用 PawnData [%s] 时尚无 Controller，其 Controller 作用域 Fragment 未生效。"),
		       *GetNameSafe(this),
		       *GetNameSafe(PawnData));
	}

	AppliedPawnData = PawnData;

	PostApplyPawnData();
}

void AModularExperiencePlayerState::RevokePawnDataFragments()
{
	if (!AppliedPawnData)
	{
		return;
	}

	PreRevokePawnData();

	const bool bHasAuthority = HasAuthority();

	if (AController* OwningController = GetOwningController())
	{
		AppliedPawnData->RevokeFragments(EPawnDataFragmentScope::Controller, MakeFragmentContext(OwningController), bHasAuthority, FragmentStates);
	}

	AppliedPawnData->RevokeFragments(EPawnDataFragmentScope::PlayerState, MakeFragmentContext(this), bHasAuthority, FragmentStates);

	FragmentStates.Reset();
	AppliedPawnData = nullptr;
}

FRotator AModularExperiencePlayerState::GetReplicatedViewRotation() const
{
	// 如需可替换为自定义 Replication 策略
	return ReplicatedViewRotation;
}

// Server：写入复制的观察视角并标记 PushModel 脏
void AModularExperiencePlayerState::SetReplicatedViewRotation(const FRotator& NewRotation)
{
	if (NewRotation != ReplicatedViewRotation)
	{
		MARK_PROPERTY_DIRTY_FROM_NAME(ThisClass, ReplicatedViewRotation, this);
		ReplicatedViewRotation = NewRotation;
	}
}

void AModularExperiencePlayerState::OnRep_PawnData()
{
	// 换装时复制到达客户端，此处先撤销旧注入再应用新注入；撤销以 AppliedPawnData 为准。
	RevokePawnDataFragments();
	ApplyPawnDataFragments();
}

void AModularExperiencePlayerState::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	RevokePawnDataFragments();

	Super::EndPlay(EndPlayReason);
}

// Replication：注册 PawnData、连接类型、观战视角与 StatTags
void AModularExperiencePlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	FDoRepLifetimeParams SharedParams;
	SharedParams.bIsPushBased = true;

	DOREPLIFETIME_WITH_PARAMS_FAST(ThisClass, PawnData, SharedParams);
	DOREPLIFETIME_WITH_PARAMS_FAST(ThisClass, MyPlayerConnectionType, SharedParams)

	SharedParams.Condition = ELifetimeCondition::COND_SkipOwner;
	DOREPLIFETIME_WITH_PARAMS_FAST(ThisClass, ReplicatedViewRotation, SharedParams);

	DOREPLIFETIME(ThisClass, StatTags);
}

void AModularExperiencePlayerState::OnExperienceLoaded(const UModularExperienceDefinition* CurrentExperience)
{
	if (const AModularExperienceGameMode* GameMode = GetWorld()->GetAuthGameMode<AModularExperienceGameMode>())
	{
		if (const UModularPawnData* NewPawnData = GameMode->GetPawnDataForController(GetOwningController()))
		{
			SetPawnData(NewPawnData);
		}
		else
		{
			UE_LOG(LogModularGameplayExperiences, Error, TEXT("AModularExperiencePlayerState::OnExperienceLoaded(): Unable to find PawnData to initialize player state [%s]!"), *GetNameSafe(this));
		}
	}
}

void AModularExperiencePlayerState::ClientInitialize(AController* Controller)
{
	Super::ClientInitialize(Controller);

	if (UModularPawnComponent* PawnComponent = UModularPawnComponent::FindModularPawnComponent(GetPawn()))
	{
		PawnComponent->CheckDefaultInitialization();
	}
}

void AModularExperiencePlayerState::PreInitializeComponents()
{
	Super::PreInitializeComponents();
}

void AModularExperiencePlayerState::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	if (const UWorld* World = GetWorld(); World && World->IsGameWorld() && World->GetNetMode() != NM_Client)
	{
		const AGameStateBase* GameState = GetWorld()->GetGameState();
		check(GameState);
		UModularExperienceComponent* ExperienceComponent = GameState->FindComponentByClass<UModularExperienceComponent>();
		check(ExperienceComponent);
		ExperienceComponent->CallOrRegister_OnExperienceLoaded(FOnModularExperienceLoaded::FDelegate::CreateUObject(this, &ThisClass::OnExperienceLoaded));
	}
}
