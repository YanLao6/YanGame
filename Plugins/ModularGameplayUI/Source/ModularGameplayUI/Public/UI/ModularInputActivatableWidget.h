#pragma once

#include "CommonActivatableWidget.h"

#include "ModularInputActivatableWidget.generated.h"

struct FUIInputConfig;

UENUM(BlueprintType)
enum class EModularInputWidgetInputMode : uint8
{
	/** 使用 CommonUI 默认输入配置（不强制覆盖）。 */
	Default,
	/** Game + Menu：游戏与菜单输入同时可用。 */
	GameAndMenu,
	/** 仅 Game 输入模式。 */
	Game,
	/** 仅 Menu 输入模式。 */
	Menu
};

// 可激活 Widget：激活时自动应用期望输入配置。
UCLASS(Abstract, Blueprintable)
class MODULARGAMEPLAYUI_API UModularInputActivatableWidget : public UCommonActivatableWidget
{
	GENERATED_BODY()

public:
	/** 构造可激活输入 Widget。 */
	UModularInputActivatableWidget(const FObjectInitializer& ObjectInitializer);

public:
	//~ UCommonActivatableWidget 接口
	/** 返回激活期间应使用的输入配置。 */
	virtual TOptional<FUIInputConfig> GetDesiredInputConfig() const override;
	//~

#if WITH_EDITOR
	/** Editor：编译 Widget 树时提示是否实现 BP_GetDesiredFocusTarget（Gamepad Focus）。 */
	virtual void ValidateCompiledWidgetTree(const UWidgetTree& BlueprintWidgetTree, class IWidgetCompilerLog& CompileLog) const override;
#endif

protected:
	/** UI 激活期间使用的输入模式（是否继续向游戏侧透传输入）。 */
	UPROPERTY(EditDefaultsOnly, Category = Input)
	EModularInputWidgetInputMode InputConfig = EModularInputWidgetInputMode::Default;

	/** 处于游戏输入时的鼠标捕获行为。 */
	UPROPERTY(EditDefaultsOnly, Category = Input)
	EMouseCaptureMode GameMouseCaptureMode = EMouseCaptureMode::CapturePermanently;
};
