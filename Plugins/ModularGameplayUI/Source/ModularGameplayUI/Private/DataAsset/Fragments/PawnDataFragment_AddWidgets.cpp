// Copyright Chronicler.

#include "DataAsset/Fragments/PawnDataFragment_AddWidgets.h"

#include "CommonUIExtensions.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(PawnDataFragment_AddWidgets)

#define LOCTEXT_NAMESPACE "PawnDataFragment_AddWidgets"

namespace PawnDataFragment_AddWidgets
{
	static ULocalPlayer* GetLocalPlayer(const FPawnDataFragmentContext& Context)
	{
		const APlayerController* PlayerController = Cast<APlayerController>(Context.Controller);
		return PlayerController ? Cast<ULocalPlayer>(PlayerController->Player) : nullptr;
	}
}

UPawnDataFragmentState* UPawnDataFragment_AddWidgets::Apply(const FPawnDataFragmentContext& Context) const
{
	ULocalPlayer* LocalPlayer = PawnDataFragment_AddWidgets::GetLocalPlayer(Context);
	if (!LocalPlayer)
	{
		// 专用服务器与非本地玩家：界面无须注入。
		return nullptr;
	}

	UPawnDataFragmentState_AddWidgets* State = NewObject<UPawnDataFragmentState_AddWidgets>(Context.TargetActor);

	for (const FPawnDataWidgetLayoutEntry& Entry : Layout)
	{
		if (const TSubclassOf<UCommonActivatableWidget> ConcreteWidgetClass = Entry.LayoutClass.LoadSynchronous())
		{
			State->LayoutsAdded.Add(UCommonUIExtensions::PushContentToLayer_ForPlayer(LocalPlayer, Entry.LayerID, ConcreteWidgetClass));
		}
	}

	if (UUIExtensionSubsystem* ExtensionSubsystem = LocalPlayer->GetWorld()->GetSubsystem<UUIExtensionSubsystem>())
	{
		for (const FPawnDataWidgetElementEntry& Entry : Widgets)
		{
			if (UClass* ConcreteWidgetClass = Entry.WidgetClass.LoadSynchronous())
			{
				State->ExtensionHandles.Add(ExtensionSubsystem->RegisterExtensionAsWidgetForContext(Entry.SlotID, LocalPlayer, ConcreteWidgetClass, Entry.Priority));
			}
		}
	}

	return State;
}

void UPawnDataFragment_AddWidgets::Revoke(const FPawnDataFragmentContext& Context, UPawnDataFragmentState* State) const
{
	UPawnDataFragmentState_AddWidgets* TypedState = Cast<UPawnDataFragmentState_AddWidgets>(State);
	if (!TypedState)
	{
		return;
	}

	for (const TWeakObjectPtr<UCommonActivatableWidget>& LayoutPtr : TypedState->LayoutsAdded)
	{
		if (UCommonActivatableWidget* AddedLayout = LayoutPtr.Get())
		{
			UCommonUIExtensions::PopContentFromLayer(AddedLayout);
		}
	}
	TypedState->LayoutsAdded.Reset();

	for (FUIExtensionHandle& Handle : TypedState->ExtensionHandles)
	{
		Handle.Unregister();
	}
	TypedState->ExtensionHandles.Reset();
}

#if WITH_EDITOR
EDataValidationResult UPawnDataFragment_AddWidgets::IsFragmentValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsFragmentValid(Context);

	int32 EntryIndex = 0;
	for (const FPawnDataWidgetLayoutEntry& Entry : Layout)
	{
		if (Entry.LayoutClass.IsNull())
		{
			Result = EDataValidationResult::Invalid;
			Context.AddError(FText::Format(LOCTEXT("NullLayoutClass", "Add Widgets Fragment 的 Layout[{0}] 未指定 LayoutClass。"), FText::AsNumber(EntryIndex)));
		}

		if (!Entry.LayerID.IsValid())
		{
			Result = EDataValidationResult::Invalid;
			Context.AddError(FText::Format(LOCTEXT("NullLayerID", "Add Widgets Fragment 的 Layout[{0}] 未指定 LayerID。"), FText::AsNumber(EntryIndex)));
		}

		++EntryIndex;
	}

	EntryIndex = 0;
	for (const FPawnDataWidgetElementEntry& Entry : Widgets)
	{
		if (Entry.WidgetClass.IsNull())
		{
			Result = EDataValidationResult::Invalid;
			Context.AddError(FText::Format(LOCTEXT("NullWidgetClass", "Add Widgets Fragment 的 Widgets[{0}] 未指定 WidgetClass。"), FText::AsNumber(EntryIndex)));
		}

		if (!Entry.SlotID.IsValid())
		{
			Result = EDataValidationResult::Invalid;
			Context.AddError(FText::Format(LOCTEXT("NullSlotID", "Add Widgets Fragment 的 Widgets[{0}] 未指定 SlotID。"), FText::AsNumber(EntryIndex)));
		}

		++EntryIndex;
	}

	return Result;
}
#endif

#undef LOCTEXT_NAMESPACE
