#include "MoverAbility/MoverAbilitySet.h"

#include "MoverComponent.h"
#include "MovementMode.h"
#include "GameFeaturesSubsystem.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MoverAbilitySet)

// K2_RegisterMove/K2_UnregisterMove 在 MinimalAPI 模块中未导出，通过 ProcessEvent 反射调用。
namespace MoverAbilitySetHelper
{
	static void RegisterLayeredMove(UMoverComponent* MoverComp, TSubclassOf<ULayeredMoveLogic> LayerClass)
	{
		static const FName FuncName(TEXT("K2_RegisterMove"));
		if (UFunction* Func = MoverComp->FindFunction(FuncName))
		{
			struct
			{
				TSubclassOf<ULayeredMoveLogic> MoveClass;
			} Params;
			Params.MoveClass = LayerClass;
			MoverComp->ProcessEvent(Func, &Params);
		}
	}

	static void UnregisterLayeredMove(UMoverComponent* MoverComp, TSubclassOf<ULayeredMoveLogic> LayerClass)
	{
		static const FName FuncName(TEXT("K2_UnregisterMove"));
		if (UFunction* Func = MoverComp->FindFunction(FuncName))
		{
			struct
			{
				TSubclassOf<ULayeredMoveLogic> MoveClass;
			} Params;
			Params.MoveClass = LayerClass;
			MoverComp->ProcessEvent(Func, &Params);
		}
	}
}

// ── FMoverAbilitySet_GrantedHandles ──────────────────────────────────────────

void FMoverAbilitySet_GrantedHandles::AddRegisteredLayer(TSubclassOf<ULayeredMoveLogic> LayerClass)
{
	RegisteredLayers.Add(LayerClass);
}

void FMoverAbilitySet_GrantedHandles::AddRegisteredTransition(UBaseMovementModeTransition* Transition, FName TargetModeName)
{
	FRegisteredModeTransitionEntry Entry;
	Entry.Transition     = Transition;
	Entry.TargetModeName = TargetModeName;
	RegisteredTransitions.Add(Entry);
}

void FMoverAbilitySet_GrantedHandles::TakeFrom(UMoverComponent* MoverComp)
{
	if (!MoverComp)
	{
		return;
	}

	for (TSubclassOf<ULayeredMoveLogic> LayerClass : RegisteredLayers)
	{
		if (LayerClass)
		{
			MoverAbilitySetHelper::UnregisterLayeredMove(MoverComp, LayerClass);
		}
	}
	RegisteredLayers.Reset();

	for (const FRegisteredModeTransitionEntry& Entry : RegisteredTransitions)
	{
		if (!Entry.Transition)
		{
			continue;
		}
		if (Entry.TargetModeName == NAME_None)
		{
			MoverComp->Transitions.RemoveSwap(Entry.Transition);
		}
		else if (UBaseMovementMode* Mode = MoverComp->FindMovementModeByName(Entry.TargetModeName))
		{
			Mode->Transitions.RemoveSwap(Entry.Transition);
		}
	}
	RegisteredTransitions.Reset();
}

// ── UMoverAbilitySet ─────────────────────────────────────────────────────────

UMoverAbilitySet::UMoverAbilitySet(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{}

void UMoverAbilitySet::GiveTo(UMoverComponent* MoverComp, FMoverAbilitySet_GrantedHandles* OutHandles) const
{
	if (!ensure(MoverComp))
	{
		return;
	}

	for (const FMoverAbilitySet_LayeredMove& Entry : GrantedLayers)
	{
		if (!Entry.LayerClass)
		{
			UE_LOG(LogGameFeatures, Warning, TEXT("[MoverAbilitySet] %s: GrantedLayers 条目缺少 LayerClass，跳过。"), *GetNameSafe(this));
			continue;
		}

		MoverAbilitySetHelper::RegisterLayeredMove(MoverComp, Entry.LayerClass);

		if (OutHandles)
		{
			OutHandles->AddRegisteredLayer(Entry.LayerClass);
		}
	}

	for (const FMoverAbilitySet_ModeTransition& Entry : GrantedTransitions)
	{
		if (!Entry.TransitionClass)
		{
			UE_LOG(LogGameFeatures, Warning, TEXT("[MoverAbilitySet] %s: GrantedTransitions 条目缺少 TransitionClass，跳过。"), *GetNameSafe(this));
			continue;
		}

		if (Entry.TargetModeTag.IsValid())
		{
			// 遍历所有已注册的 MovementMode，注册到第一个匹配 Tag 的 Mode
			UBaseMovementMode* TargetMode     = nullptr;
			FName              TargetModeName = NAME_None;
			for (const TPair<FName, TObjectPtr<UBaseMovementMode>>& Pair : MoverComp->MovementModes)
			{
				if (Pair.Value && Pair.Value->HasGameplayTag(Entry.TargetModeTag, false))
				{
					TargetMode     = Pair.Value;
					TargetModeName = Pair.Key;
					break;
				}
			}

			if (!TargetMode)
			{
				UE_LOG(LogGameFeatures, Warning, TEXT("[MoverAbilitySet] %s: 未找到携带 Tag '%s' 的 MovementMode，Transition 跳过。"), *GetNameSafe(this), *Entry.TargetModeTag.ToString());
				continue;
			}

			UBaseMovementModeTransition* Transition = NewObject<UBaseMovementModeTransition>(TargetMode, Entry.TransitionClass);
			TargetMode->Transitions.AddUnique(Transition);

			if (OutHandles)
			{
				OutHandles->AddRegisteredTransition(Transition, TargetModeName);
			}
		}
		else
		{
			UBaseMovementModeTransition* Transition = NewObject<UBaseMovementModeTransition>(MoverComp, Entry.TransitionClass);
			MoverComp->Transitions.AddUnique(Transition);

			if (OutHandles)
			{
				OutHandles->AddRegisteredTransition(Transition, NAME_None);
			}
		}
	}
}
