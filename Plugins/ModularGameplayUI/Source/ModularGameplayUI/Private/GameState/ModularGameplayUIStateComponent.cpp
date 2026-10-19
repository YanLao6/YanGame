// Copyright Epic Games, Inc. All Rights Reserved.

#include "GameState/ModularGameplayUIStateComponent.h"

#include "CommonGameInstance.h"
#include "CommonSessionSubsystem.h"
#include "CommonUserSubsystem.h"
#include "ControlFlowManager.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Kismet/GameplayStatics.h"
#include "NativeGameplayTags.h"
#include "PrimaryGameLayout.h"
#include "TimerManager.h"
#include "ActorComponent/ModularExperienceComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ModularGameplayUIStateComponent)

DEFINE_LOG_CATEGORY_STATIC(LogModularGameplayUIFlow, Log, All);

namespace UITags
{
	UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_PLATFORM_TRAIT_SINGLEONLINEUSER, "Platform.Trait.SingleOnlineUser");
	UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_UI_LAYER_GAME, "UI.Layer.Game");
	UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_UI_LAYER_GAMEMENU, "UI.Layer.GameMenu");
	UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_UI_LAYER_MENU, "UI.Layer.Menu");
	UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_UI_LAYER_MODAL, "UI.Layer.Modal");
}

UModularGameplayUIStateComponent::UModularGameplayUIStateComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UModularGameplayUIStateComponent::BeginPlay()
{
	Super::BeginPlay();

	// 等待 Experience 加载完成
	const AGameStateBase* GameState = GetGameStateChecked<AGameStateBase>();
	UModularExperienceComponent* ExperienceComponent = GameState->FindComponentByClass<UModularExperienceComponent>();
	check(ExperienceComponent);

	// Delegate 挂在同生命周期 Component 上，无需在 EndPlay 手动解绑
	ExperienceComponent->CallOrRegister_OnExperienceLoaded_HighPriority(
		FOnModularExperienceLoaded::FDelegate::CreateUObject(this, &ThisClass::OnExperienceLoaded));
}

void UModularGameplayUIStateComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);
}

bool UModularGameplayUIStateComponent::ShouldShowLoadingScreen(FString& OutReason) const
{
	if (bShouldShowLoadingScreen)
	{
		OutReason = TEXT("UI Flow Pending...");
		
		if (UIFlow.IsValid())
		{
			if (const TOptional<FString> StepDebugName = UIFlow->GetCurrentStepDebugName();
				StepDebugName.IsSet())
			{
				OutReason = StepDebugName.GetValue();
			}
		}
		
		return true;
	}

	return false;
}

void UModularGameplayUIStateComponent::OnExperienceLoaded(const UModularExperienceDefinition* Experience)
{
	FControlFlow& Flow = FControlFlowStatics::Create(this, TEXT("UIFlow"))
		.QueueStep(TEXT("Wait For User Initialization"), this, &ThisClass::FlowStep_WaitForUserInitialization)
		.QueueStep(TEXT("Try Show Press Start Screen"), this, &ThisClass::FlowStep_TryShowPressStartScreen)
		.QueueStep(TEXT("Try Join Requested Session"), this, &ThisClass::FlowStep_TryJoinRequestedSession)
		.QueueStep(TEXT("Try Show Main Screen"), this, &ThisClass::FlowStep_TryShowMainScreen);

	Flow.ExecuteFlow();

	UIFlow = Flow.AsShared();
}

void UModularGameplayUIStateComponent::FlowStep_WaitForUserInitialization(const FControlFlowNodeRef SubFlow)
{
	// 若为重连/硬断开，清理 User 与 Session 状态
	// TODO：引擎断开流程应更明确地区分原因，便于此处分支
	bool bWasHardDisconnect = false;
	const UGameInstance* GameInstance = UGameplayStatics::GetGameInstance(this);

	// GameMode 仅存在于 authority 端；客户端（如前端以 Client 模式运行）没有 GameMode，
	// 此处不再对其 ensure，硬断开判定交由 authority 完成即可
	if (GetOwner()->HasAuthority())
	{
		const AGameModeBase* GameMode = GetWorld()->GetAuthGameMode<AGameModeBase>();
		if (ensure(GameMode) && UGameplayStatics::HasOption(GameMode->OptionsString, TEXT("closed")))
		{
			bWasHardDisconnect = true;
		}
	}

	// 仅在硬断开时重置本地 User 状态
	UCommonUserSubsystem* UserSubsystem = GameInstance->GetSubsystem<UCommonUserSubsystem>();
	if (ensure(UserSubsystem) && bWasHardDisconnect)
	{
		UserSubsystem->ResetUserState();
	}

	// 始终清理 Session 子系统挂起状态
	UCommonSessionSubsystem* SessionSubsystem = GameInstance->GetSubsystem<UCommonSessionSubsystem>();
	if (ensure(SessionSubsystem))
	{
		SessionSubsystem->CleanUpSessions();
	}

	SubFlow->ContinueFlow();
}

void UModularGameplayUIStateComponent::FlowStep_TryShowPressStartScreen(FControlFlowNodeRef SubFlow)
{
	const UGameInstance* GameInstance = UGameplayStatics::GetGameInstance(this);
	UCommonUserSubsystem* UserSubsystem = GameInstance->GetSubsystem<UCommonUserSubsystem>();

	// 1P 已登录则可跳过 Press Start
	if (const UCommonUserInfo* FirstUser = UserSubsystem->GetUserInfoForLocalPlayerIndex(0))
	{
		if (FirstUser->InitializationState == ECommonUserInitializationState::LoggedInLocalOnly ||
			FirstUser->InitializationState == ECommonUserInitializationState::LoggedInOnline)
		{
			SubFlow->ContinueFlow();
			return;
		}
	}

	// 平台是否需要「按 Start 选择用户」；多在线用户平台常需要，单用户平台可走自动初始化
	if (!UserSubsystem->ShouldWaitForStartInput())
	{
		// 自动初始化本地 play（使用默认 InputDeviceId）
		InProgressPressStartScreen = SubFlow;
		UserSubsystem->OnUserInitializeComplete.AddDynamic(this, &UModularGameplayUIStateComponent::OnUserInitialized);
		UserSubsystem->TryToInitializeForLocalPlay(0, FInputDeviceId(), false);

		return;
	}

	// 推入 Press Start 界面；其 Deactivate 后继续 Flow
	UPrimaryGameLayout* RootLayout = UPrimaryGameLayout::GetPrimaryGameLayoutForPrimaryPlayer(this);
	if (RootLayout == nullptr)
	{
		// Client 时序：主玩家 PlayerState 或根布局尚未就绪，下一帧重试而非静默丢弃 Flow
		UE_LOG(LogModularGameplayUIFlow, Verbose, TEXT("Press Start 步骤：RootLayout 未就绪，下一帧重试"));
		GetWorld()->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this, [this, SubFlow]()
		{
			FlowStep_TryShowPressStartScreen(SubFlow);
		}));
		return;
	}

	constexpr bool bSuspendInputUntilComplete = true;
	RootLayout->PushWidgetToLayerStackAsync<UCommonActivatableWidget>(UITags::TAG_UI_LAYER_MENU, bSuspendInputUntilComplete, PressStartScreenClass,
		[this, SubFlow](const EAsyncWidgetLayerState State, const UCommonActivatableWidget* Screen) {
		switch (State)
		{
		case EAsyncWidgetLayerState::AfterPush:
			bShouldShowLoadingScreen = false;
			Screen->OnDeactivated().AddWeakLambda(this, [this, SubFlow]() {
				SubFlow->ContinueFlow();
			});
			break;
		case EAsyncWidgetLayerState::Canceled:
			bShouldShowLoadingScreen = false;
			SubFlow->ContinueFlow();
			return;
		default: ;
		}
	});
}

void UModularGameplayUIStateComponent::OnUserInitialized(const UCommonUserInfo* UserInfo, bool bSuccess, FText Error, ECommonUserPrivilege RequestedPrivilege, ECommonUserOnlineContext OnlineContext)
{
	const FControlFlowNodePtr FlowToContinue = InProgressPressStartScreen;
	const UGameInstance* GameInstance = UGameplayStatics::GetGameInstance(this);

	if (UCommonUserSubsystem* UserSubsystem = GameInstance->GetSubsystem<UCommonUserSubsystem>();
		ensure(FlowToContinue.IsValid() && UserSubsystem))
	{
		UserSubsystem->OnUserInitializeComplete.RemoveDynamic(this, &UModularGameplayUIStateComponent::OnUserInitialized);
		InProgressPressStartScreen.Reset();

		if (bSuccess)
		{
			FlowToContinue->ContinueFlow();
		}
		else
		{
			// TODO：后续可跳转错误 UI；当前先继续 Flow
			FlowToContinue->ContinueFlow();
		}
	}
}

void UModularGameplayUIStateComponent::FlowStep_TryJoinRequestedSession(FControlFlowNodeRef SubFlow)
{
	if (UCommonGameInstance* GameInstance = Cast<UCommonGameInstance>(UGameplayStatics::GetGameInstance(this));
		GameInstance->GetRequestedSession() != nullptr && GameInstance->CanJoinRequestedSession())
	{
		if (UCommonSessionSubsystem* SessionSubsystem = GameInstance->GetSubsystem<UCommonSessionSubsystem>();
			ensure(SessionSubsystem))
		{
			// Join Session 完成：成功则取消后续主菜单 Flow；失败则进入主菜单
			// TODO：需确保 Join 完成后 ServerTravel 等后续链路完备
			OnJoinSessionCompleteEventHandle = SessionSubsystem->OnJoinSessionCompleteEvent.AddWeakLambda(this, [this, SubFlow, SessionSubsystem](const FOnlineResultInformation& Result)
			{
				// 事件源为 SessionSubsystem，此时仍有效
				SessionSubsystem->OnJoinSessionCompleteEvent.Remove(OnJoinSessionCompleteEventHandle);
				OnJoinSessionCompleteEventHandle.Reset();

				if (Result.bWasSuccessful)
				{
					SubFlow->CancelFlow();
				}
				else
				{
					SubFlow->ContinueFlow();
					return;
				}
			});
			GameInstance->JoinRequestedSession();
			return;
		}
	}
	// 未发起 Join 则跳过本步
	SubFlow->ContinueFlow();
}

void UModularGameplayUIStateComponent::FlowStep_TryShowMainScreen(FControlFlowNodeRef SubFlow)
{
	UPrimaryGameLayout* RootLayout = UPrimaryGameLayout::GetPrimaryGameLayoutForPrimaryPlayer(this);
	if (RootLayout == nullptr)
	{
		// 同 Press Start：布局未就绪时下一帧重试，避免 Client 下 Flow 悬停
		UE_LOG(LogModularGameplayUIFlow, Verbose, TEXT("Main Screen 步骤：RootLayout 未就绪，下一帧重试"));
		GetWorld()->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this, [this, SubFlow]()
		{
			FlowStep_TryShowMainScreen(SubFlow);
		}));
		return;
	}

	constexpr bool bSuspendInputUntilComplete = true;
	RootLayout->PushWidgetToLayerStackAsync<UCommonActivatableWidget>(UITags::TAG_UI_LAYER_MENU, bSuspendInputUntilComplete, MainScreenClass,
		[this, SubFlow](EAsyncWidgetLayerState State, UCommonActivatableWidget* Screen) {
		switch (State)
		{
		case EAsyncWidgetLayerState::AfterPush:
			bShouldShowLoadingScreen = false;
			SubFlow->ContinueFlow();
			return;
		case EAsyncWidgetLayerState::Canceled:
			bShouldShowLoadingScreen = false;
			SubFlow->ContinueFlow();
			return;
		default: ;
		}
	});
}
