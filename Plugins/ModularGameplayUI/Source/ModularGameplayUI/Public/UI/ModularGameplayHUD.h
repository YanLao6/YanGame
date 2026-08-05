// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ModularLayoutInterface.h"
#include "GameFramework/HUD.h"
#include "ModularUserWidget.h"
#include "ModularWidgetController.h"
#include "ModularGameplayHUD.generated.h"

#define UE_API MODULARGAMEPLAYUI_API


class UAttributeSet;
class UModularAbilitySystemComponent;
class AModularPlayerState;
class AModularPlayerController;
struct FWidgetControllerParams;

namespace EEndPlayReason { enum Type : int; }

class AActor;
class UObject;
class UModularUserWidget;
struct FTimerHandle;

/**
 * Modular Gameplay HUD。
 *
 * 在标准 HUD 基础上承载 GameplayLayout 与 OverlayWidgetController，
 * 用于 Experience 驱动的 UI 装配流程。
 */
UCLASS(MinimalAPI, Config = Game)
class AModularGameplayHUD : public AHUD, public IModularLayoutInterface
{
	GENERATED_BODY()

public:
	/** 构造 HUD 并初始化基础 UI 状态。 */
	UE_API explicit AModularGameplayHUD(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	UPROPERTY()
	TObjectPtr<UModularUserWidget> OverlayWidget;
	
	/** 获取并初始化 Overlay 的 WidgetController。 */
	UE_API UModularWidgetController* GetOverlayWidgetController(const FWidgetControllerParams& WCParams);

	/** 初始化 Overlay Widget 与其能力系统上下文。 */
	UE_API void InitOverlay(AModularPlayerController* PC, AModularPlayerState* PS, UModularAbilitySystemComponent* ASC,const UAttributeSet* AS);

	UPROPERTY(EditAnywhere)
	TSubclassOf<UModularUserWidget> OverlayWidgetClass;

	UPROPERTY()
	TObjectPtr<UModularWidgetController> OverlayWidgetController;

	UPROPERTY(EditAnywhere)
	TSubclassOf<UModularWidgetController> OverlayWidgetControllerClass;

	/** 获取当前挂载的 GameplayLayout。 */
	virtual UModularGameplayLayout* GetModularGameplayLayout() const override { return ModularGameplayLayout; }
	/** 设置当前 HUD 使用的 GameplayLayout。 */
	UE_API virtual void SetModularGameplayLayout(UModularGameplayLayout* InLayout) override;

protected:

	//~ UObject 接口
	UE_API virtual void PreInitializeComponents() override;
	//~

	//~ AActor 接口
	UE_API virtual void BeginPlay() override;
	UE_API virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	//~

	//~ AHUD 接口
	UE_API virtual void GetDebugActorList(TArray<AActor*>& InOutList) override;
	//~

	UPROPERTY()
	UModularGameplayLayout* ModularGameplayLayout;

private:
	// 在 ModularGameplayLayout 尚未就绪时用于重试 Overlay 初始化的 Timer
	FTimerHandle OverlayInitRetryTimerHandle;
	
};

#undef UE_API
