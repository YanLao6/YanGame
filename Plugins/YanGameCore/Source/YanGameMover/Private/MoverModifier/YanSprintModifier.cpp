#include "MoverModifier/YanSprintModifier.h"

#include "ChaosMover/ChaosMoverSimulation.h"
#include "ChaosMover/ChaosMoverSimulationTypes.h"
#include "MoverSimulationTypes.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(YanSprintModifier)

UE_DEFINE_GAMEPLAY_TAG(Mover_IsSprinting, "Mover.IsSprinting");

FYanSprintModifier::FYanSprintModifier()
{
	// 持续状态：由 UChaosCharacterSprintCheck 在退出条件成立时显式取消
	DurationMs = -1.0f;
}

bool FYanSprintModifier::HasGameplayTag(FGameplayTag TagToFind, bool bExactMatch) const
{
	return bExactMatch ? TagToFind.MatchesTagExact(Mover_IsSprinting) : TagToFind.MatchesTag(Mover_IsSprinting);
}

void FYanSprintModifier::GetGameplayTags(FGameplayTagContainer& InOutTags) const
{
	InOutTags.AddTag(Mover_IsSprinting);
}

void FYanSprintModifier::OnStart_Async(const FMovementModifierParams_Async& Params)
{
	if (!Params.IsValid())
	{
		return;
	}

	UChaosMoverSimulation* Simulation = Cast<UChaosMoverSimulation>(Params.Simulation);
	if (!Simulation)
	{
		return;
	}

	const FName ModeName = Params.SyncState->MovementMode;
	if (IChaosCharacterMovementModeInterface* CharacterMode =
			Cast<IChaosCharacterMovementModeInterface>(Simulation->FindMovementModeByName_Mutable(ModeName).Get()))
	{
		// 无条件重应用：override 存放在 Mode 对象上而非 SyncState，回滚不会将其还原
		CharacterMode->OverrideMaxSpeed(MaxSpeedOverride);
		CharacterMode->OverrideAcceleration(AccelerationOverride);
		AppliedModeName = ModeName;
	}
}

void FYanSprintModifier::OnEnd_Async(const FMovementModifierParams_Async& Params)
{
	if (!Params.IsValid() || AppliedModeName.IsNone())
	{
		return;
	}

	UChaosMoverSimulation* Simulation = Cast<UChaosMoverSimulation>(Params.Simulation);
	if (!Simulation)
	{
		return;
	}

	if (IChaosCharacterMovementModeInterface* CharacterMode =
			Cast<IChaosCharacterMovementModeInterface>(Simulation->FindMovementModeByName_Mutable(AppliedModeName).Get()))
	{
		CharacterMode->ClearMaxSpeedOverride();
		CharacterMode->ClearAccelerationOverride();
	}

	AppliedModeName = NAME_None;
}

FMovementModifierBase* FYanSprintModifier::Clone() const
{
	return new FYanSprintModifier(*this);
}

void FYanSprintModifier::NetSerialize(FArchive& Ar)
{
	Super::NetSerialize(Ar);

	Ar << MaxSpeedOverride;
	Ar << AccelerationOverride;
	Ar << AppliedModeName;
}

UScriptStruct* FYanSprintModifier::GetScriptStruct() const
{
	return FYanSprintModifier::StaticStruct();
}

FString FYanSprintModifier::ToSimpleString() const
{
	return FString::Printf(TEXT("Yan Sprint Modifier"));
}
