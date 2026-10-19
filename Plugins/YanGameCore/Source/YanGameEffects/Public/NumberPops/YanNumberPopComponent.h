#pragma once

#include "Components/ControllerComponent.h"
#include "GameplayTagContainer.h"

#include "YanNumberPopComponent.generated.h"

#define UE_API YANGAMEEFFECTS_API

/** 一次飘字请求。 */
USTRUCT(BlueprintType)
struct FYanNumberPopRequest
{
	GENERATED_BODY()

	// 飘字生成的世界位置
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Yan|NumberPops")
	FVector WorldLocation;

	// 来源侧标签，用于挑选样式
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Yan|NumberPops")
	FGameplayTagContainer SourceTags;

	// 目标侧标签，用于挑选样式
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Yan|NumberPops")
	FGameplayTagContainer TargetTags;

	// 要显示的数值
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Yan|NumberPops")
	int32 NumberToDisplay = 0;

	// 是否为暴击
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Yan|NumberPops")
	bool bIsCriticalDamage = false;

	FYanNumberPopRequest()
		: WorldLocation(ForceInitToZero)
	{}
};

/**
 * 伤害飘字组件的基类，挂在 Controller 上，只在本地玩家侧生成表现。
 * 具体呈现方式由子类决定：Niagara 粒子或静态网格文字。
 */
UCLASS(MinimalAPI, Abstract)
class UYanNumberPopComponent : public UControllerComponent
{
	GENERATED_BODY()

public:
	UE_API UYanNumberPopComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** 提交一次飘字请求 */
	UFUNCTION(BlueprintCallable, Category = "Yan|NumberPops")
	virtual void AddNumberPop(const FYanNumberPopRequest& NewRequest) {}
};

#undef UE_API
