// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UI/ModularButtonBase.h"
#include "UI/ModularTabListWidgetBase.h"

#include "ModularTabButtonBase.generated.h"

#define UE_API MODULARGAMEPLAYUI_API

class UCommonLazyImage;
class UObject;
struct FSlateBrush;

/** 页签按钮：标题与图标由所属页签列表在创建时下发 */
UCLASS(MinimalAPI, Abstract, Blueprintable, meta = (DisableNativeTick))
class UModularTabButtonBase : public UModularButtonBase, public IModularTabButtonInterface
{
	GENERATED_BODY()

public:
	UE_API void SetIconFromLazyObject(TSoftObjectPtr<UObject> LazyObject);
	UE_API void SetIconBrush(const FSlateBrush& Brush);

protected:
	//~Begin IModularTabButtonInterface
	UFUNCTION()
	UE_API virtual void SetTabLabelInfo_Implementation(const FModularTabDescriptor& TabLabelInfo) override;
	//~End IModularTabButtonInterface

private:
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UCommonLazyImage> LazyImage_Icon;
};

#undef UE_API
