// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/ModularTabButtonBase.h"

#include "CommonLazyImage.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ModularTabButtonBase)

void UModularTabButtonBase::SetIconFromLazyObject(TSoftObjectPtr<UObject> LazyObject)
{
	if (LazyImage_Icon)
	{
		LazyImage_Icon->SetBrushFromLazyDisplayAsset(LazyObject);
	}
}

void UModularTabButtonBase::SetIconBrush(const FSlateBrush& Brush)
{
	if (LazyImage_Icon)
	{
		LazyImage_Icon->SetBrush(Brush);
	}
}

void UModularTabButtonBase::SetTabLabelInfo_Implementation(const FModularTabDescriptor& TabLabelInfo)
{
	SetButtonText(TabLabelInfo.TabText);
	SetIconBrush(TabLabelInfo.IconBrush);
}
