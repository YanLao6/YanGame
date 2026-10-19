#pragma once

#include "Actors/ColorButton.h"

#include "ColorFuseCrystal.generated.h"

#define UE_API COLORTRANSFERRUNTIME_API

class AColorWindZone;

/**
 * 引信晶体：沿用按钮的颜料池混色，调出 TargetMix 后碎裂，碎裂时关闭 WindsToDeactivate 并销毁自身。
 *
 * 不放出颜料。
 */
UCLASS(MinimalAPI)
class AColorFuseCrystal : public AColorButton
{
	GENERATED_BODY()

public:
	UE_API AColorFuseCrystal();

protected:
	//~Begin AColorButton Interface
	UE_API virtual void AuthOnBroken() override;
	//~End AColorButton Interface

	/** 碎裂时关闭的风场 */
	UPROPERTY(EditInstanceOnly, Category = "Yan|ColorTransfer")
	TArray<TObjectPtr<AColorWindZone>> WindsToDeactivate;
};

#undef UE_API
