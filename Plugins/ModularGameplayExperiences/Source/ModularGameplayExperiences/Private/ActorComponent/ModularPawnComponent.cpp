// Copyright Chronicler.


#include "ActorComponent/ModularPawnComponent.h"
#include "ModularGameplayExperiencesLogs.h"
#include "ModularGameplayTags.h"
#include "Components/GameFrameworkComponentDelegates.h"
#include "Components/GameFrameworkComponentManager.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "Net/UnrealNetwork.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ModularPawnComponent)

class FLifetimeProperty;
class UActorComponent;

const FName UModularPawnComponent::NAME_ActorFeatureName("ModularPawn");

UModularPawnComponent::UModularPawnComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bStartWithTickEnabled = false;
	PrimaryComponentTick.bCanEverTick          = false;

	SetIsReplicatedByDefault(true);

	PawnData = nullptr;
}

void UModularPawnComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UModularPawnComponent, PawnData);
}

void UModularPawnComponent::OnRegister()
{
	Super::OnRegister();

	const APawn* Pawn = GetPawn<APawn>();
	ensureAlwaysMsgf((Pawn != nullptr),
	                 TEXT("ModularPawnComponent on [%s] can only be added to Pawn actors."),
	                 *GetNameSafe(GetOwner()));

	TArray<UActorComponent*> ModularPawnComponents;
	Pawn->GetComponents(UModularPawnComponent::GetClass(), ModularPawnComponents);
	ensureAlwaysMsgf((ModularPawnComponents.Num() == 1),
	                 TEXT("Only one ModularPawnComponent should exist on [%s]."),
	                 *GetNameSafe(GetOwner()));

	// 尽早注册到 GameFramework InitState（仅游戏 World 生效）
	RegisterInitStateFeature();
}

void UModularPawnComponent::BeginPlay()
{
	Super::BeginPlay();

	// 监听其它 Feature 的 InitState 变化
	BindOnActorInitStateChanged(NAME_None, FGameplayTag(), false);

	// 标记 Spawned 并尝试沿默认链推进
	ensure(TryToChangeInitState(ModularGameplayTags::InitState_Spawned));
	CheckDefaultInitialization();
}

void UModularPawnComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	RevokePawnDataFragments();

	UnregisterInitStateFeature();

	Super::EndPlay(EndPlayReason);
}

FPawnDataFragmentContext UModularPawnComponent::MakeFragmentContext() const
{
	APawn* Pawn = const_cast<UModularPawnComponent*>(this)->GetPawn<APawn>();

	FPawnDataFragmentContext Context;
	Context.TargetActor = Pawn;
	Context.Pawn        = Pawn;
	Context.Controller  = Pawn ? Pawn->GetController() : nullptr;

	return Context;
}

void UModularPawnComponent::ApplyPawnDataFragments()
{
	if (AppliedPawnData || !PawnData)
	{
		return;
	}

	const APawn* Pawn = GetPawn<APawn>();
	if (!Pawn)
	{
		return;
	}

	PawnData->ApplyFragments(EPawnDataFragmentScope::Pawn, MakeFragmentContext(), Pawn->HasAuthority(), FragmentStates);

	AppliedPawnData = PawnData;
}

void UModularPawnComponent::RevokePawnDataFragments()
{
	if (!AppliedPawnData)
	{
		return;
	}

	const APawn* Pawn = GetPawn<APawn>();

	AppliedPawnData->RevokeFragments(EPawnDataFragmentScope::Pawn, MakeFragmentContext(), Pawn && Pawn->HasAuthority(), FragmentStates);

	FragmentStates.Reset();
	AppliedPawnData = nullptr;
}

void UModularPawnComponent::SetPawnData(const UModularPawnData* InPawnData)
{
	check(InPawnData);

	APawn* Pawn = GetPawnChecked<APawn>();

	if (Pawn->GetLocalRole() != ROLE_Authority)
	{
		return;
	}

	if (PawnData)
	{
		UE_LOG(LogModularGameplayExperiences, Error, TEXT("Trying to set PawnData [%s] on pawn [%s] that already has valid PawnData [%s]."), *GetNameSafe(InPawnData), *GetNameSafe(Pawn), *GetNameSafe(PawnData));
		return;
	}

	PawnData = InPawnData;

	Pawn->ForceNetUpdate();

	CheckDefaultInitialization();
}

void UModularPawnComponent::HandleControllerChanged()
{
	CheckDefaultInitialization();
}

void UModularPawnComponent::OnRep_PawnData()
{
	CheckDefaultInitialization();
}

void UModularPawnComponent::HandlePlayerStateReplicated()
{
	CheckDefaultInitialization();
}

void UModularPawnComponent::SetupPlayerInputComponent()
{
	CheckDefaultInitialization();
}

void UModularPawnComponent::CheckDefaultInitialization()
{
	// 先推进依赖的其它 Feature，再检查本 Feature
	CheckDefaultInitializationForImplementers();

	static const TArray<FGameplayTag> StateChain =
	{
		ModularGameplayTags::InitState_Spawned,
		ModularGameplayTags::InitState_DataAvailable,
		ModularGameplayTags::InitState_DataInitialized,
		ModularGameplayTags::InitState_GameplayReady
	};

	// 自 Spawned（BeginPlay 设置）起沿链推进至 GameplayReady
	ContinueInitStateChain(StateChain);
}

bool UModularPawnComponent::CanChangeInitState(UGameFrameworkComponentManager* Manager, FGameplayTag CurrentState, FGameplayTag DesiredState) const
{
	check(Manager);

	APawn* Pawn = GetPawn<APawn>();
	if (!CurrentState.IsValid() && DesiredState == ModularGameplayTags::InitState_Spawned)
	{
		// 有效 Pawn 即视为 Spawned
		if (Pawn)
		{
			return true;
		}
	}
	if (CurrentState == ModularGameplayTags::InitState_Spawned && DesiredState == ModularGameplayTags::InitState_DataAvailable)
	{
		// 必须具备 PawnData
		if (!PawnData)
		{
			return false;
		}

		const bool bHasAuthority        = Pawn->HasAuthority();
		const bool bIsLocallyControlled = Pawn->IsLocallyControlled();

		if (bHasAuthority || bIsLocallyControlled)
		{
			// Authority 或本地控制：需要已被 Controller Possess
			if (!GetController<AController>())
			{
				return false;
			}
		}

		return true;
	}
	else if (CurrentState == ModularGameplayTags::InitState_DataAvailable && DesiredState == ModularGameplayTags::InitState_DataInitialized)
	{
		// 当本 Actor 上所有 Feature 均达 DataAvailable 后进入 DataInitialized
		return Manager->HaveAllFeaturesReachedInitState(Pawn, ModularGameplayTags::InitState_DataAvailable);
	}
	else if (CurrentState == ModularGameplayTags::InitState_DataInitialized && DesiredState == ModularGameplayTags::InitState_GameplayReady)
	{
		return true;
	}

	return false;
}

void UModularPawnComponent::HandleChangeInitState(UGameFrameworkComponentManager* Manager, FGameplayTag CurrentState, FGameplayTag DesiredState)
{
	if (DesiredState == ModularGameplayTags::InitState_DataInitialized)
	{
		// PawnData 已可用且 Controller 已配对，此时注入 Pawn 作用域 Fragment。
		// 其余初始化逻辑由监听该状态的其它组件完成。
		ApplyPawnDataFragments();
	}
}

void UModularPawnComponent::OnActorInitStateChanged(const FActorInitStateChangedParams& Params)
{
	// 其它 Feature 到达 DataAvailable 时，尝试推进本 Feature
	if (Params.FeatureName != NAME_ActorFeatureName)
	{
		if (Params.FeatureState == ModularGameplayTags::InitState_DataAvailable)
		{
			CheckDefaultInitialization();
		}
	}
}
