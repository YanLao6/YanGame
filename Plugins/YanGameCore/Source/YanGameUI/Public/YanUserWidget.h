// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UI/ModularUserWidget.h"
#include "YanUserWidget.generated.h"

/**
 * 
 */
UCLASS(Abstract, Blueprintable)
class YANGAMEUI_API UYanUserWidget : public UModularUserWidget
{
	GENERATED_BODY()

public:
	UYanUserWidget(const FObjectInitializer& ObjectInitializer);
};
