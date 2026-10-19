#pragma once

#include "CommonUserWidget.h"
#include "Performance/YanPerformanceStatTypes.h"

#include "YanPerfStatContainerBase.generated.h"

#define UE_API YANGAMEUI_API

/**
 * 承载一组性能统计控件的面板，按玩家设置控制各子控件的可见性。
 * 一个面板只负责一种展示形式，文字与曲线通常分别放在两个面板中。
 */
UCLASS(MinimalAPI, Abstract)
class UYanPerfStatContainerBase : public UCommonUserWidget
{
	GENERATED_BODY()

public:
	UE_API UYanPerfStatContainerBase(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	//~Begin UUserWidget Interface
	UE_API virtual void NativeConstruct() override;
	UE_API virtual void NativeDestruct() override;
	//~End UUserWidget Interface

	/** 依据当前设置刷新所有子统计控件的可见性 */
	UFUNCTION(BlueprintCallable, Category = "Yan|Performance")
	UE_API void UpdateVisibilityOfChildren();

protected:
	// 本面板负责的展示形式
	UPROPERTY(EditAnywhere, Category = Display)
	EYanStatDisplayMode StatDisplayModeFilter = EYanStatDisplayMode::TextAndGraph;
};

#undef UE_API
