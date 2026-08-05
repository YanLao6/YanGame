#pragma once
#include "CommonActivatableWidget.h"
#include "ModularInputActivatableWidget.h"
#include "ModularUserWidget.h"
#include "ModularGameplayLayout.generated.h"

/**
 * Gameplay 子布局基类。
 *
 * 用于挂入 PrimaryGameLayout 的各个 Layer，支持 Escape 菜单等常驻交互入口。
 */
UCLASS(MinimalAPI, Abstract, BlueprintType, Blueprintable, Meta = (DisplayName = "Modular Gameplay Layout", Category = "HUD"))
class UModularGameplayLayout : public UModularUserWidget
{
	GENERATED_BODY()

public:
	/** 构造 GameplayLayout。 */
	UModularGameplayLayout(const FObjectInitializer& ObjectInitializer);

protected:
	UPROPERTY(EditDefaultsOnly, Category = Gameplay)
	TSoftClassPtr<UCommonActivatableWidget> EscapeMenuClass;

	/** 响应 Escape UIAction：推送 EscapeMenuClass 定义的界面到 Menu Layer。 */
	void HandleEscapeAction();

public:
	/** 初始化时绑定 Escape 输入动作。 */
	virtual void NativeOnInitialized() override;
	/** 析构时解绑输入动作。 */
	virtual void NativeDestruct() override;
};
