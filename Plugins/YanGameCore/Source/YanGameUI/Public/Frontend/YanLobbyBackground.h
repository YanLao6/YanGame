// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Engine/DataAsset.h"

#include "YanLobbyBackground.generated.h"

class UObject;
class UWorld;

UCLASS(config=EditorPerProjectUserSettings)
class YANGAMEUI_API UYanLobbyBackground : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Yan|UI")
	TSoftObjectPtr<UWorld> BackgroundLevel;
};
