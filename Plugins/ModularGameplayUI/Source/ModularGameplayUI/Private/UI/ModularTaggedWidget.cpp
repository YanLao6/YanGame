// Copyright Chronicler.

#include "UI/ModularTaggedWidget.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "ActorComponent/ModularAbilityExtensionComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ModularTaggedWidget)

UModularTaggedWidget::UModularTaggedWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UModularTaggedWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (!IsDesignTime())
	{
		if (!HiddenByTags.IsEmpty())
		{
			if (APlayerController* PlayerController = GetOwningPlayer())
			{
				WatchedController = PlayerController;
				PlayerController->OnPossessedPawnChanged.AddUniqueDynamic(this, &ThisClass::HandlePossessedPawnChanged);
				BindToPawn(PlayerController->GetPawn());
			}
		}

		// 以当前可见性为输入走一遍标签判定，确定初始显隐。
		SetVisibility(GetVisibility());
	}
}

void UModularTaggedWidget::NativeDestruct()
{
	if (APlayerController* PlayerController = WatchedController.Get())
	{
		PlayerController->OnPossessedPawnChanged.RemoveDynamic(this, &ThisClass::HandlePossessedPawnChanged);
	}
	WatchedController.Reset();

	// 扩展组件不提供注销接口，置空 WatchedPawn 使其后续初始化通知失效。
	WatchedPawn.Reset();
	UnbindFromAbilitySystem();

	Super::NativeDestruct();
}

void UModularTaggedWidget::SetVisibility(ESlateVisibility InVisibility)
{
#if WITH_EDITORONLY_DATA
	if (IsDesignTime())
	{
		Super::SetVisibility(InVisibility);
		return;
	}
#endif

	// 记录调用方意图：即使当前被标签压制，标签解除后仍应恢复到这里请求的可见性。
	bWantsToBeVisible = ConvertSerializedVisibilityToRuntime(InVisibility).IsVisible();
	if (bWantsToBeVisible)
	{
		ShownVisibility = InVisibility;
	}
	else
	{
		HiddenVisibility = InVisibility;
	}

	ApplyDesiredVisibility();
}

void UModularTaggedWidget::HandlePossessedPawnChanged(APawn* OldPawn, APawn* NewPawn)
{
	BindToPawn(NewPawn);
}

void UModularTaggedWidget::BindToPawn(APawn* Pawn)
{
	WatchedPawn = Pawn;

	if (Pawn)
	{
		// 客户端 possess 可能早于 PlayerState 复制，此时 Pawn 暴露的 ASC 仍为空。
		if (UModularAbilityExtensionComponent* AbilityExtension = Pawn->FindComponentByClass<UModularAbilityExtensionComponent>())
		{
			AbilityExtension->OnAbilitySystemInitialized_RegisterAndCall(
				FSimpleMulticastDelegate::FDelegate::CreateUObject(this, &ThisClass::HandleAbilitySystemInitialized));
		}
	}

	BindToAbilitySystem(UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Pawn));
}

void UModularTaggedWidget::HandleAbilitySystemInitialized()
{
	// 旧 Pawn 的扩展组件仍持有本回调，统一以当前 WatchedPawn 为准。
	if (APawn* Pawn = WatchedPawn.Get())
	{
		BindToAbilitySystem(UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Pawn));
	}
}

void UModularTaggedWidget::BindToAbilitySystem(UAbilitySystemComponent* AbilitySystem)
{
	if (WatchedAbilitySystem.Get() != AbilitySystem)
	{
		UnbindFromAbilitySystem();

		if (AbilitySystem)
		{
			WatchedAbilitySystem = AbilitySystem;
			for (const FGameplayTag& Tag : HiddenByTags)
			{
				const FDelegateHandle Handle = AbilitySystem->RegisterGameplayTagEvent(Tag, EGameplayTagEventType::NewOrRemoved)
					.AddUObject(this, &ThisClass::OnWatchedTagsChanged);
				WatchedTagHandles.Emplace(Tag, Handle);
			}
		}
	}

	ApplyDesiredVisibility();
}

void UModularTaggedWidget::UnbindFromAbilitySystem()
{
	if (UAbilitySystemComponent* AbilitySystem = WatchedAbilitySystem.Get())
	{
		for (const TPair<FGameplayTag, FDelegateHandle>& TagHandle : WatchedTagHandles)
		{
			AbilitySystem->UnregisterGameplayTagEvent(TagHandle.Value, TagHandle.Key, EGameplayTagEventType::NewOrRemoved);
		}
	}

	WatchedTagHandles.Reset();
	WatchedAbilitySystem.Reset();
}

void UModularTaggedWidget::OnWatchedTagsChanged(const FGameplayTag Tag, int32 NewCount)
{
	ApplyDesiredVisibility();
}

void UModularTaggedWidget::ApplyDesiredVisibility()
{
	const UAbilitySystemComponent* AbilitySystem = WatchedAbilitySystem.Get();
	const bool bHasHiddenTags = AbilitySystem && AbilitySystem->HasAnyMatchingGameplayTags(HiddenByTags);

	const ESlateVisibility DesiredVisibility = (bWantsToBeVisible && !bHasHiddenTags) ? ShownVisibility : HiddenVisibility;
	if (GetVisibility() != DesiredVisibility)
	{
		Super::SetVisibility(DesiredVisibility);
	}
}
