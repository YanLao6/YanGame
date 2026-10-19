#include "GameMode/ColorTransferHeroAssignmentComponent.h"

#include "ColorTransferRuntimeModule.h"
#include "DataAsset/ModularPawnData.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"
#include "ModularTeamAgentInterface.h"
#include "ModularTeamSubsystem.h"
#include "Player/ModularExperiencePlayerState.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ColorTransferHeroAssignmentComponent)

bool UColorTransferHeroAssignmentComponent::IsManagingHeroAssignment() const
{
	return !TeamHeroes.IsEmpty() || Super::IsManagingHeroAssignment();
}

const UModularPawnData* UColorTransferHeroAssignmentComponent::ChooseHeroForController(const AController* Controller)
{
	const UModularTeamSubsystem* TeamSubsystem = GetWorld()->GetSubsystem<UModularTeamSubsystem>();
	const int32 TeamId = TeamSubsystem ? TeamSubsystem->FindTeamFromObject(Controller) : INDEX_NONE;

	if (TeamId == INDEX_NONE)
	{
		if (IModularTeamAgentInterface* TeamAgent = Cast<IModularTeamAgentInterface>(Controller->PlayerState))
		{
			TeamAgent->GetTeamChangedDelegateChecked().AddUniqueDynamic(this, &ThisClass::OnPlayerTeamChanged);
		}
		return nullptr;
	}

	if (const TObjectPtr<UModularPawnData>* TeamHero = TeamHeroes.Find(TeamId); TeamHero && *TeamHero)
	{
		return *TeamHero;
	}

	UE_LOG(LogColorTransfer, Warning, TEXT("%s 所在队伍 %d 没有配置角色，退回默认分配"), *GetNameSafe(Controller), TeamId);
	return Super::ChooseHeroForController(Controller);
}

void UColorTransferHeroAssignmentComponent::OnPlayerTeamChanged(UObject* ObjectChangingTeam, int32 OldTeamID, int32 NewTeamID)
{
	AModularExperiencePlayerState* PlayerState = Cast<AModularExperiencePlayerState>(ObjectChangingTeam);
	AController* Controller = PlayerState ? PlayerState->GetOwningController() : nullptr;
	if (!Controller || NewTeamID == INDEX_NONE)
	{
		return;
	}

	Cast<IModularTeamAgentInterface>(PlayerState)->GetTeamChangedDelegateChecked().RemoveDynamic(this, &ThisClass::OnPlayerTeamChanged);

	AssignHeroForController(Controller);

	// 玩家初始化时的 PawnData 补写发生在队伍落定之前，须在此补上，否则 Pawn 会以空 PawnData 生成
	if (const UModularPawnData* Hero = ResolveHeroForPlayer(Controller))
	{
		PlayerState->SetPawnData(Hero);
	}
}
