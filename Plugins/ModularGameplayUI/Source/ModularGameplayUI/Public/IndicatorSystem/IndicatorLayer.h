// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/Widget.h"

#include "IndicatorLayer.generated.h"

class SActorCanvas;
class SWidget;
class UObject;

/**
 * 指示器图层 Widget。
 *
 * 承载 SActorCanvas，将所有屏幕指示器统一绘制在该图层上。
 */
UCLASS()
class UIndicatorLayer : public UWidget
{
	GENERATED_UCLASS_BODY()

public:
	/** 当指示器被钳制到屏幕边缘并需要显示箭头时使用的默认箭头笔刷。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Appearance)
	FSlateBrush ArrowBrush;

protected:
	//~Begin UWidget Interface
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;
	virtual TSharedRef<SWidget> RebuildWidget() override;
	//~End UWidget Interface

protected:
	TSharedPtr<SActorCanvas> MyActorCanvas;
};
