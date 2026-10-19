// 圆周标记控件声明。

#pragma once

#include "CoreMinimal.h"
#include "Components/Widget.h"
#include "Weapon/SCircumferenceMarkerWidget.h"
#include "CircumferenceMarkerWidget.generated.h"

#define UE_API EQUIPMANAGER_API

class SWidget;

/**
 * 圆周标记控件。
 *
 * 供准星表现散布：半径随武器当前散布角变化，标记随之扩散或收拢。
 */
UCLASS(MinimalAPI)
class UCircumferenceMarkerWidget : public UWidget
{
	GENERATED_BODY()

public:
	UE_API UCircumferenceMarkerWidget(const FObjectInitializer& ObjectInitializer);

	//~Begin UWidget Interface
	UE_API virtual void SynchronizeProperties() override;
protected:
	UE_API virtual TSharedRef<SWidget> RebuildWidget() override;
	//~End UWidget Interface

public:
	//~Begin UVisual Interface
	UE_API virtual void ReleaseSlateResources(bool bReleaseChildren) override;
	//~End UVisual Interface

	/** 各标记所处的位置与朝向。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Appearance)
	TArray<FCircumferenceMarkerEntry> MarkerList;

	/** 圆周半径。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Appearance, meta=(ClampMin=0.0))
	float Radius = 48.0f;

	/** 单个标记的图像。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Appearance)
	FSlateBrush MarkerImage;

	/** 标记贴在散布半径之外还是之内。 */
	UPROPERTY(EditAnywhere, Category=Corner)
	uint8 bReticleCornerOutsideSpreadRadius : 1;

	/** 设置圆周半径。 */
	UFUNCTION(BlueprintCallable, Category = "Appearance")
	UE_API void SetRadius(float InRadius);

private:
	TSharedPtr<SCircumferenceMarkerWidget> MyMarkerWidget;
};

#undef UE_API
