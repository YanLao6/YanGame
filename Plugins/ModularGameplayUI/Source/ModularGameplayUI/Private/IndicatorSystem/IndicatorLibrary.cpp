// Copyright Epic Games, Inc. All Rights Reserved.

#include "IndicatorSystem/IndicatorLibrary.h"

#include "IndicatorSystem/IndicatorManagerComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(IndicatorLibrary)

class AController;

UIndicatorLibrary::UIndicatorLibrary()
{
}

UIndicatorManagerComponent* UIndicatorLibrary::GetIndicatorManagerComponent(AController* Controller)
{
	return UIndicatorManagerComponent::GetComponent(Controller);
}
