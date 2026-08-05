// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UI/ModularUserWidget.h"
#include "YanUserWidget.generated.h"

#define UE_API YANGAMEUI_API

/**
 * 
 */
UCLASS(MinimalAPI, Abstract, Blueprintable)
class UYanUserWidget : public UModularUserWidget
{
	GENERATED_BODY()

public:
	UE_API UYanUserWidget(const FObjectInitializer& ObjectInitializer);
};

#undef UE_API
