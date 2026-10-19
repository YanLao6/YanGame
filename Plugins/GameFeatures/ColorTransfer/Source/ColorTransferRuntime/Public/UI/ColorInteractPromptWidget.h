#pragma once

#include "GameplayTagContainer.h"
#include "YanUserWidget.h"

#include "ColorInteractPromptWidget.generated.h"

#define UE_API COLORTRANSFERRUNTIME_API

class UTextBlock;

/**
 * 交互提示：由交互技能在有可交互物时挂到 HUD 扩展点，只显示交互输入当前映射的按键名。
 *
 * 按键在构造时经 UModularExperienceUtils::GetKeyValueByTag 局内解析，改键后下次出现即生效；
 * 未映射任何按键时文本为空。
 *
 * UMG 布局需绑定（变量名需一致）：
 *   UTextBlock KeyText —— 按键名文本
 */
UCLASS(MinimalAPI, Abstract)
class UColorInteractPromptWidget : public UYanUserWidget
{
	GENERATED_BODY()

protected:
	//~Begin UUserWidget Interface
	UE_API virtual void NativeConstruct() override;
	//~End UUserWidget Interface

	/** 交互输入 Tag，须与技能集中交互技能所绑的一致 */
	UPROPERTY(EditDefaultsOnly, Category = "Yan|Interaction", Meta = (Categories = "InputTag"))
	FGameplayTag InputTag;

	UPROPERTY(Meta = (BindWidget))
	TObjectPtr<UTextBlock> KeyText;
};

#undef UE_API
