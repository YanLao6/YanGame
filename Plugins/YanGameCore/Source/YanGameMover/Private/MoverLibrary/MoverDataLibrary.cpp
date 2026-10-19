#include "MoverLibrary/MoverDataLibrary.h"

#include "MoverComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MoverDataLibrary)

bool UMoverDataLibrary::GetDefaultSyncState(const FMoverDataCollection& Collection, FMoverDefaultSyncState& OutSyncState)
{
	if (const FMoverDefaultSyncState* Found = Collection.FindDataByType<FMoverDefaultSyncState>())
	{
		OutSyncState = *Found;
		return true;
	}

	return false;
}

bool UMoverDataLibrary::GetDefaultInputs(const FMoverDataCollection& Collection, FCharacterDefaultInputs& OutInputs)
{
	if (const FCharacterDefaultInputs* Found = Collection.FindDataByType<FCharacterDefaultInputs>())
	{
		OutInputs = *Found;
		return true;
	}

	return false;
}

bool UMoverDataLibrary::GetYanCharacterInputs(const FMoverDataCollection& Collection, FYanCharacterInputs& OutInputs)
{
	if (const FYanCharacterInputs* Found = Collection.FindDataByType<FYanCharacterInputs>())
	{
		OutInputs = *Found;
		return true;
	}

	return false;
}

FCharacterDefaultInputs UMoverDataLibrary::GetOrAddDefaultInputs(FMoverInputCmdContext& InputCmd)
{
	return InputCmd.InputCollection.FindOrAddMutableDataByType<FCharacterDefaultInputs>();
}

void UMoverDataLibrary::CommitDefaultInputs(FMoverInputCmdContext& InputCmd, const FCharacterDefaultInputs& Inputs)
{
	FCharacterDefaultInputs& Stored = InputCmd.InputCollection.FindOrAddMutableDataByType<FCharacterDefaultInputs>();
	Stored                          = Inputs;
}

FYanCharacterInputs UMoverDataLibrary::GetOrAddYanInputs(FMoverInputCmdContext& InputCmd)
{
	return InputCmd.InputCollection.FindOrAddMutableDataByType<FYanCharacterInputs>();
}

void UMoverDataLibrary::CommitYanInputs(FMoverInputCmdContext& InputCmd, const FYanCharacterInputs& Inputs)
{
	FYanCharacterInputs& Stored = InputCmd.InputCollection.FindOrAddMutableDataByType<FYanCharacterInputs>();
	Stored                      = Inputs;
}

bool UMoverDataLibrary::GetFatigueEntry(const UMoverComponent* MoverComp, EYanFatigueChannel Channel, FYanFatigueEntry& OutEntry)
{
	if (!MoverComp)
	{
		return false;
	}
	const FYanFatigueEntry* Entry = FYanFatigueState::FindEntry(MoverComp->GetSyncState(), Channel);
	if (!Entry)
	{
		return false;
	}
	OutEntry = *Entry;
	return true;
}
