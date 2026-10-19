// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Blueprint/UserWidget.h"
#include "CoreMinimal.h"
#include "GameFramework/SpectatorPawn.h"
#include "GameplayTagContainer.h"
#include "UIExtensionSystem.h"

#include "YanSpectatorPawn.generated.h"

#define UE_API YANGAMEPLAY_API

class AYanPlayerController;
class UInputAction;
class UInputMappingContext;
class UUserWidget;

/**
 * 观战摄影机 Pawn。
 *
 * 仅存在于本地客户端且不参与复制，承载自由飞行与观战目标切换的输入。
 * 移动逻辑沿用 ADefaultPawn，此处只把 EnhancedInput 接到既有函数上。
 */
UCLASS(MinimalAPI, Blueprintable)
class AYanSpectatorPawn : public ASpectatorPawn
{
	GENERATED_BODY()

public:
	UE_API explicit AYanSpectatorPawn(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

protected:
	//~Begin AActor Interface
	UE_API virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	//~End AActor Interface

	//~Begin APawn Interface
	UE_API virtual void PawnClientRestart() override;
	UE_API virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	//~End APawn Interface

	/**
	 * 观战期间生效的输入映射，随本 Pawn 的生灭增删。
	 *
	 * 优先级须高于角色映射：进入观战不会清空角色映射，同键位会被前者遮蔽。
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Yan|Spectate|Input")
	TArray<TObjectPtr<UInputMappingContext>> SpectateInputMappings;

	UPROPERTY(EditDefaultsOnly, Category = "Yan|Spectate|Input")
	int32 SpectateInputPriority = 100;

	/** 水平移动，X 为左右、Y 为前后。 */
	UPROPERTY(EditDefaultsOnly, Category = "Yan|Spectate|Input")
	TObjectPtr<UInputAction> MoveAction;

	/** 视角转动，X 为偏航、Y 为俯仰。 */
	UPROPERTY(EditDefaultsOnly, Category = "Yan|Spectate|Input")
	TObjectPtr<UInputAction> LookAction;

	/** 世界空间升降。 */
	UPROPERTY(EditDefaultsOnly, Category = "Yan|Spectate|Input")
	TObjectPtr<UInputAction> MoveVerticalAction;

	/** 按住加速。 */
	UPROPERTY(EditDefaultsOnly, Category = "Yan|Spectate|Input")
	TObjectPtr<UInputAction> SprintAction;

	UPROPERTY(EditDefaultsOnly, Category = "Yan|Spectate|Input")
	TObjectPtr<UInputAction> NextTargetAction;

	UPROPERTY(EditDefaultsOnly, Category = "Yan|Spectate|Input")
	TObjectPtr<UInputAction> PreviousTargetAction;

	/** 在跟随目标与自由视角之间切换。 */
	UPROPERTY(EditDefaultsOnly, Category = "Yan|Spectate|Input")
	TObjectPtr<UInputAction> ToggleFreeCameraAction;

	UPROPERTY(EditDefaultsOnly, Category = "Yan|Spectate|Input")
	TObjectPtr<UInputAction> LeaveSpectateAction;

	/** 加速时的速度倍率。 */
	UPROPERTY(EditDefaultsOnly, Category = "Yan|Spectate", meta = (ClampMin = "1.0"))
	float SprintSpeedMultiplier = 4.0f;

	/** 观战界面，随本 Pawn 插入/撤出 HUD 扩展点；留空则不显示界面。 */
	UPROPERTY(EditDefaultsOnly, Category = "Yan|Spectate|UI")
	TSubclassOf<UUserWidget> SpectateWidgetClass;

	/**
	 * 观战界面挂载的 HUD 扩展点。
	 *
	 * 观战条是叠加在 HUD 上的元素而非独立界面，走扩展点而非 CommonUI 层栈：
	 * 层栈一次只显示栈顶，压进去会与 HUD 互相顶掉。
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Yan|Spectate|UI", meta = (Categories = "HUD.Slot"))
	FGameplayTag SpectateWidgetSlot;

private:
	void Input_Move(const struct FInputActionValue& InputActionValue);
	void Input_Look(const struct FInputActionValue& InputActionValue);
	void Input_MoveVertical(const struct FInputActionValue& InputActionValue);
	void Input_SprintStart();
	void Input_SprintStop();
	void Input_NextTarget();
	void Input_PreviousTarget();
	void Input_ToggleFreeCamera();
	void Input_LeaveSpectate();

	AYanPlayerController* GetYanPlayerController() const;

	// 观战输入映射的增删，仅对本地控制端有意义
	void ApplySpectateInputMappings(bool bApply);

	void RegisterSpectateWidget();
	void UnregisterSpectateWidget();

	// 加速前的基准速度，供松开时还原
	float BaseMaxSpeed = 0.0f;

	FUIExtensionHandle SpectateWidgetHandle;
};

#undef UE_API
