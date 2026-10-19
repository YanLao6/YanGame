#include "MoverModifier/YanMaxSpeedScaleModifier.h"

#include "ChaosMover/ChaosMoverSimulation.h"
#include "ChaosMover/ChaosMoverSimulationTypes.h"
#include "MoverSimulationTypes.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(YanMaxSpeedScaleModifier)

UE_DEFINE_GAMEPLAY_TAG(Mover_HasMaxSpeedScale, "Mover.HasMaxSpeedScale");

FYanMaxSpeedScaleModifier::FYanMaxSpeedScaleModifier()
{
	// 持续状态：由排队方持句柄显式取消
	DurationMs = -1.0f;
}

bool FYanMaxSpeedScaleModifier::HasGameplayTag(FGameplayTag TagToFind, bool bExactMatch) const
{
	return bExactMatch ? TagToFind.MatchesTagExact(Mover_HasMaxSpeedScale) : TagToFind.MatchesTag(Mover_HasMaxSpeedScale);
}

void FYanMaxSpeedScaleModifier::GetGameplayTags(FGameplayTagContainer& InOutTags) const
{
	InOutTags.AddTag(Mover_HasMaxSpeedScale);
}

void FYanMaxSpeedScaleModifier::OnStart_Async(const FMovementModifierParams_Async& Params)
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
		// 读取当前模式速度再折算：无条件重应用，override 存放在 Mode 对象上而非 SyncState，回滚不会将其还原
		CharacterMode->OverrideMaxSpeed(CharacterMode->GetMaxSpeed() * SpeedScale);
		AppliedModeName = ModeName;
	}
}

void FYanMaxSpeedScaleModifier::OnEnd_Async(const FMovementModifierParams_Async& Params)
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
	}

	AppliedModeName = NAME_None;
}

FMovementModifierBase* FYanMaxSpeedScaleModifier::Clone() const
{
	return new FYanMaxSpeedScaleModifier(*this);
}

void FYanMaxSpeedScaleModifier::NetSerialize(FArchive& Ar)
{
	Super::NetSerialize(Ar);

	Ar << SpeedScale;
	Ar << AppliedModeName;
}

UScriptStruct* FYanMaxSpeedScaleModifier::GetScriptStruct() const
{
	return FYanMaxSpeedScaleModifier::StaticStruct();
}

FString FYanMaxSpeedScaleModifier::ToSimpleString() const
{
	return FString::Printf(TEXT("Yan Max Speed Scale Modifier (x%.2f)"), SpeedScale);
}
