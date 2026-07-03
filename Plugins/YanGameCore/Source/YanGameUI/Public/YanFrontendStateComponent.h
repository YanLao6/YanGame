#pragma once

#include "CoreMinimal.h"
#include "GameState/ModularGameplayUIStateComponent.h"

#include "YanFrontendStateComponent.generated.h"


class UModularExperienceDefinition;
class UCommonActivatableWidget;
enum class ECommonUserOnlineContext : uint8;
enum class ECommonUserPrivilege : uint8;
class UCommonUserInfo;

/**
 * 前端状态组件（Frontend State Component）
 *
 * 用于在前端（如主菜单/Press Start 等）阶段驱动一段可控的 UI Flow，并与 Experience 加载、
 * 用户初始化（CommonUser）以及会话请求（CommonSession）进行衔接。
 *
 * 说明：
 * - 通过 `ILoadingProcessInterface` 控制是否展示 Loading Screen。
 * - Flow 的每一步在 `FlowStep_*` 中完成，必要时可异步等待 UI/网络回调后再推进。
 */
UCLASS(Abstract)
class YANGAMEUI_API UYanFrontendStateComponent : public UModularGameplayUIStateComponent
{
	GENERATED_BODY()

public:
	/** 构造函数。通常由 `AGameStateBase` 挂载并随其生命周期存在。 */
	UYanFrontendStateComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
};
