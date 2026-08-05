// Fill out your copyright notice in the Description page of Project Settings.


#include "Hero/YanPawnComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(YanPawnComponent)


// Sets default values for this component's properties
UYanPawnComponent::UYanPawnComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = false;
}


void UYanPawnComponent::BeginPlay()
{
	Super::BeginPlay();
}
