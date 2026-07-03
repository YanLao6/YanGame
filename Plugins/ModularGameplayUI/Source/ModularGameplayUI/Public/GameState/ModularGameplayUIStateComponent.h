// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CommonActivatableWidget.h"
#include "CommonUserSubsystem.h"
#include "Components/GameStateComponent.h"
#include "ControlFlowNode.h"
#include "LoadingProcessInterface.h"
#include "GameMode/ModularExperienceDefinition.h"

#include "ModularGameplayUIStateComponent.generated.h"

/**
 * 可扩展的 GameStateComponent：用属性配置驱动一套 UI 流程（Press Start / Join Session / Main）。
 *
 * 依赖基于 CommonGame 的 GameUIPolicy 与 PrimaryGameLayout；
 * Layout 须在运行时注册 Layer Widget，并为 Gameplay Layer Tag `UI.Layer.Menu` 等提供对应 Layer。
 *
 * @see ModularGameplayUI.h 用法概览
 * @see UPrimaryGameLayout::RegisterLayer
 *
 * @todo Layout 缺少某 Layer Tag 时当前可能崩溃，需在 Layer 注册侧补齐校验。
 */
UCLASS(Abstract)
class MODULARGAMEPLAYUI_API UModularGameplayUIStateComponent : public UGameStateComponent, public ILoadingProcessInterface
{
	GENERATED_BODY()

public:

	/** 构造并初始化默认 UI Flow 状态。 */
	UModularGameplayUIStateComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	//~ UActorComponent 接口
	/** 启动时订阅 Experience 加载完成。 */
	virtual void BeginPlay() override;
	/** 组件结束时与基类一致清理（当前无额外解绑，Experience Delegate 随 Component 销毁）。 */
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	//~

	//~ ILoadingProcessInterface 接口
	/** Loading Screen 是否仍应显示（UI Flow 未完成时返回 true）。 */
	virtual bool ShouldShowLoadingScreen(FString& OutReason) const override;
	//~

private:
	// Experience 加载完成：构建并执行 ControlFlow
	void OnExperienceLoaded(const UModularExperienceDefinition* Experience);

	UFUNCTION()
	void OnUserInitialized(const UCommonUserInfo* UserInfo, bool bSuccess, FText Error, ECommonUserPrivilege RequestedPrivilege, ECommonUserOnlineContext OnlineContext);

	void FlowStep_WaitForUserInitialization(const FControlFlowNodeRef SubFlow);
	void FlowStep_TryShowPressStartScreen(FControlFlowNodeRef SubFlow);
	void FlowStep_TryJoinRequestedSession(FControlFlowNodeRef SubFlow);
	void FlowStep_TryShowMainScreen(FControlFlowNodeRef SubFlow);

	// UI Flow 未完成前保持 Loading Screen
	bool bShouldShowLoadingScreen = true;

	UPROPERTY(EditAnywhere, Category = UI)
	TSoftClassPtr<UCommonActivatableWidget> PressStartScreenClass;

	UPROPERTY(EditAnywhere, Category = UI)
	TSoftClassPtr<UCommonActivatableWidget> MainScreenClass;

	TSharedPtr<FControlFlow> UIFlow;
	
	// Press Start 自动登录流程进行中时持有的 Flow 节点
	FControlFlowNodePtr InProgressPressStartScreen;

	FDelegateHandle OnJoinSessionCompleteEventHandle;
};
