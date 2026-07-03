// Copyright Chronicler.

#include "../Public/ModalCameraComponent.h"

#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "..\Public\ModalCameraMode.h"
#include "ModularGameplayTags.h"
#include "ModularPlayerState.h"
#include "ActorComponent/ModularPawnComponent.h"
#include "Components/GameFrameworkComponentManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ModalCameraComponent)

const FName UModalCameraComponent::NAME_ActorFeatureName("ModalCamera");

UModalCameraComponent::UModalCameraComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	CameraModeStack = nullptr;
	FieldOfViewOffset = 0.0f;
}


void UModalCameraComponent::SetAbilityCameraMode(TSubclassOf<UModalCameraMode> CameraMode,
	const FGameplayAbilitySpecHandle& OwningSpecHandle)
{
	if (CameraMode)
	{
		AbilityCameraMode = CameraMode;
		AbilityCameraModeOwningSpecHandle = OwningSpecHandle;
	}
}

void UModalCameraComponent::ClearAbilityCameraMode(const FGameplayAbilitySpecHandle& OwningSpecHandle)
{
	if (AbilityCameraModeOwningSpecHandle == OwningSpecHandle)
	{
		AbilityCameraMode = nullptr;
		AbilityCameraModeOwningSpecHandle = FGameplayAbilitySpecHandle();
	}
}

void UModalCameraComponent::SetCinematicCameraMode(TSubclassOf<UModalCameraMode> CameraMode)
{
	CinematicCameraMode = CameraMode;
}

void UModalCameraComponent::ClearCinematicCameraMode()
{
	CinematicCameraMode = nullptr;
}

void UModalCameraComponent::SetDebugCameraMode(TSubclassOf<UModalCameraMode> CameraMode)
{
	DebugCameraMode = CameraMode;
}

void UModalCameraComponent::ClearDebugCameraMode()
{
		DebugCameraMode = nullptr;
}

void UModalCameraComponent::BeginPlay()
{
	Super::BeginPlay();

	// 监听 Pawn 扩展组件 InitState 变化
	BindOnActorInitStateChanged(NAME_ActorFeatureName, FGameplayTag(), false);

	// 标记 Spawned 完成并尝试继续初始化链
	ensure(TryToChangeInitState(ModularGameplayTags::InitState_Spawned));
	CheckDefaultInitialization();
}

void UModalCameraComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UnregisterInitStateFeature();

	Super::EndPlay(EndPlayReason);
}

void UModalCameraComponent::OnRegister()
{
	Super::OnRegister();

	if (!CameraModeStack)
	{
		CameraModeStack = NewObject<UCameraModeStack>(this);
		check(CameraModeStack);
	}
}

void UModalCameraComponent::GetCameraView(float DeltaTime, FMinimalViewInfo& DesiredView)
{
	check(CameraModeStack);

	UpdateCameraModes();

	FCameraModeView CameraModeView;
	CameraModeStack->EvaluateStack(DeltaTime, CameraModeView);

	// PlayerController ControlRotation 与相机 View 同步
	if (const APawn* TargetPawn = Cast<APawn>(GetTargetActor()))
	{
		if (APlayerController* PC = TargetPawn->GetController<APlayerController>())
		{
			PC->SetControlRotation(CameraModeView.ControlRotation);
		}
	}

	// 叠加单帧 FOV 偏移后清零
	CameraModeView.FieldOfView += FieldOfViewOffset;
	FieldOfViewOffset = 0.0f;

	// UCameraComponent 与最新 View 对齐
	SetWorldLocationAndRotation(CameraModeView.Location, CameraModeView.Rotation);
	FieldOfView = CameraModeView.FieldOfView;

	// 写入 FMinimalViewInfo 供渲染/逻辑使用
	DesiredView.Location = CameraModeView.Location;
	DesiredView.Rotation = CameraModeView.Rotation;
	DesiredView.FOV = CameraModeView.FieldOfView;
	DesiredView.OrthoWidth = OrthoWidth;
	DesiredView.OrthoNearClipPlane = OrthoNearClipPlane;
	DesiredView.OrthoFarClipPlane = OrthoFarClipPlane;
	DesiredView.AspectRatio = AspectRatio;
	DesiredView.bConstrainAspectRatio = bConstrainAspectRatio;
	DesiredView.bUseFieldOfViewForLOD = bUseFieldOfViewForLOD;
	DesiredView.ProjectionMode = ProjectionMode;

	// PostProcess：按组件权重混入后处理设置
	DesiredView.PostProcessBlendWeight = PostProcessBlendWeight;
	if (PostProcessBlendWeight > 0.0f)
	{
		DesiredView.PostProcessSettings = PostProcessSettings;
	}


	if (IsXRHeadTrackedCamera())
	{
		// XR：头戴追踪下上述多数逻辑不适用，但 PostProcess 仍可能需基类处理
		Super::GetCameraView(DeltaTime, DesiredView);
	}
}

void UModalCameraComponent::UpdateCameraModes()
{
	check(CameraModeStack);

	if (CameraModeStack->IsStackActivate())
	{
		if (DetermineCameraModeDelegate.IsBound())
		{
			if (const TSubclassOf<UModalCameraMode> CameraMode{DetermineCameraModeDelegate.Execute()})
			{
				CameraModeStack->PushCameraMode(CameraMode);
			}
		}
	}
}

void UModalCameraComponent::DrawDebug(UCanvas* Canvas) const
{
	check(Canvas);

	FDisplayDebugManager& DisplayDebugManager = Canvas->DisplayDebugManager;

	DisplayDebugManager.SetFont(GEngine->GetSmallFont());
	DisplayDebugManager.SetDrawColor(FColor::Yellow);
	DisplayDebugManager.DrawString(FString::Printf(TEXT("ModalCameraComponent: %s"), *GetNameSafe(GetTargetActor())));

	DisplayDebugManager.SetDrawColor(FColor::White);
	DisplayDebugManager.DrawString(FString::Printf(TEXT("   Location: %s"), *GetComponentLocation().ToCompactString()));
	DisplayDebugManager.DrawString(FString::Printf(TEXT("   Rotation: %s"), *GetComponentRotation().ToCompactString()));
	DisplayDebugManager.DrawString(FString::Printf(TEXT("   FOV: %f"), FieldOfView));
	DisplayDebugManager.DrawString(FString::Printf(TEXT("    Mode: %s"), *GetNameSafe(DetermineCameraMode())));

	check(CameraModeStack);
	CameraModeStack->DrawDebug(Canvas);
}

void UModalCameraComponent::GetBlendInfo(float& OutWeightOfTopLayer, FGameplayTag& OutTagOfTopLayer) const
{
	check(CameraModeStack);
	CameraModeStack->GetBlendInfo(/*out*/ OutWeightOfTopLayer, /*out*/ OutTagOfTopLayer);
}

bool UModalCameraComponent::CanChangeInitState(UGameFrameworkComponentManager* Manager, FGameplayTag CurrentState,
	FGameplayTag DesiredState) const
{
	check(Manager);

	APawn* Pawn = GetPawn<APawn>();

	if (!CurrentState.IsValid() && DesiredState == ModularGameplayTags::InitState_Spawned)
	{
		// 存在有效 Pawn 则允许进入 Spawned
		if (Pawn)
		{
			return true;
		}
	}
	else if (CurrentState == ModularGameplayTags::InitState_Spawned
		&& DesiredState == ModularGameplayTags::InitState_DataAvailable)
	{
		const bool bIsLocallyControlled = Pawn->IsLocallyControlled();

		if (const bool bIsBot = Pawn->IsBotControlled(); bIsLocallyControlled && !bIsBot)
		{
			// 本地操控：需要 InputComponent、PlayerController 与 LocalPlayer
			if (const APlayerController* PC = GetController<APlayerController>();
				!Pawn->InputComponent || !PC || !PC->GetLocalPlayer())
			{
				return false;
			}
		}

		return true;
	}
	else if (CurrentState == ModularGameplayTags::InitState_DataAvailable
		&& DesiredState == ModularGameplayTags::InitState_DataInitialized)
	{
		// 等待 PlayerState 与 ModularPawnComponent 初始化完成
		const AModularPlayerState* PS = GetPlayerState<AModularPlayerState>();

		return PS && Manager->HasFeatureReachedInitState(Pawn, UModularPawnComponent::NAME_ActorFeatureName, ModularGameplayTags::InitState_DataInitialized);
	}
	else if (CurrentState == ModularGameplayTags::InitState_DataInitialized
		&& DesiredState == ModularGameplayTags::InitState_GameplayReady)
	{
		// @todo 可在此加入 Ability 初始化条件
		return true;
	}

	return false;
}

void UModalCameraComponent::HandleChangeInitState(UGameFrameworkComponentManager* Manager, FGameplayTag CurrentState,
	FGameplayTag DesiredState)
{
	if (CurrentState == ModularGameplayTags::InitState_DataAvailable
		&& DesiredState == ModularGameplayTags::InitState_DataInitialized)
	{
		const APawn* Pawn = GetPawn<APawn>();
		if (!ensure(Pawn))
		{
			return;
		}
		// 为 Pawn 绑定 DetermineCameraMode（观战切换后仍能解析）
		// @todo 允许由 PawnData 等覆盖绑定方式
		if (UModalCameraComponent* CameraComponent = FindCameraComponent(Pawn))
		{
			CameraComponent->DetermineCameraModeDelegate.BindUObject(this, &ThisClass::DetermineCameraMode);
		}
	}
}

void UModalCameraComponent::OnActorInitStateChanged(const FActorInitStateChangedParams& Params)
{
	if (Params.FeatureName == NAME_ActorFeatureName)
	{
		if (Params.FeatureState == ModularGameplayTags::InitState_DataInitialized)
		{
			// 扩展组件报告就绪后尝试推进 Init 链
			CheckDefaultInitialization();
		}
	}
}

void UModalCameraComponent::CheckDefaultInitialization()
{
	static const TArray<FGameplayTag> StateChain =
	{
		ModularGameplayTags::InitState_Spawned,
		ModularGameplayTags::InitState_DataAvailable,
		ModularGameplayTags::InitState_DataInitialized,
		ModularGameplayTags::InitState_GameplayReady
	};

	// 自 BeginPlay 设的 Spawned 起，沿链推进直至 GameplayReady
	ContinueInitStateChain(StateChain);
}

TSubclassOf<UCameraMode> UModalCameraComponent::DetermineCameraMode() const
{
	if (DebugCameraMode) return DebugCameraMode;

	CustomCameraDelegate.Broadcast(CustomCameraModeStack);
	for (int i = 0; i < CustomCameraModeStack.Num(); i++)
	{
		if (i == CustomCameraModeStack.Num() - 1)
		{
			return CustomCameraModeStack[i]->GetClass();
		}
		if (const TSubclassOf<UModalCameraMode> CustomCameraMode{CustomCameraModeStack[i]->GetClass()})
		{
			CameraModeStack->PushCameraMode(CustomCameraMode->GetClass());
		}
	}

	if (CinematicCameraMode) return CinematicCameraMode;

	if (AbilityCameraMode) return AbilityCameraMode;

	if (const APawn* Pawn = GetPawn<APawn>(); !Pawn)
	{
		return nullptr;
	}
	return DefaultCameraMode;
}
