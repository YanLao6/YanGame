// 加载屏子系统声明。

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Templates/SubclassOf.h"
#include "YanLoadingScreenSubsystem.generated.h"

#define UE_API YANGAMEUI_API

class UUserWidget;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FLoadingScreenWidgetChangedDelegate, TSubclassOf<UUserWidget>, NewWidgetClass);

/**
 * 加载屏子系统。
 *
 * 保存当前加载屏应展示的内容控件，其生命周期跨关卡切换，
 * 使切图期间的加载界面配置不随旧关卡一同销毁。
 */
UCLASS(MinimalAPI)
class UYanLoadingScreenSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	UE_API UYanLoadingScreenSubsystem();

	/** 设置加载屏宿主内要展示的内容控件。 */
	UFUNCTION(BlueprintCallable)
	UE_API void SetLoadingScreenContentWidget(TSubclassOf<UUserWidget> NewWidgetClass);

	/**
	 * 按 Experience 配置的软类引用异步加载后切换加载屏内容；引用为空时立即清空，
	 * 交由加载屏宿主 Widget 自身的兜底逻辑回退到默认内容。
	 */
	UFUNCTION(BlueprintCallable)
	UE_API void PrepareLoadingScreenForExperience(TSoftClassPtr<UUserWidget> ExperienceLoadingScreenWidget);

	/** 返回最近一次设置的加载屏内容控件。 */
	UFUNCTION(BlueprintPure)
	UE_API TSubclassOf<UUserWidget> GetLoadingScreenContentWidget() const;

private:
	UPROPERTY(BlueprintAssignable, meta=(AllowPrivateAccess))
	FLoadingScreenWidgetChangedDelegate OnLoadingScreenWidgetChanged;

	UPROPERTY()
	TSubclassOf<UUserWidget> LoadingScreenWidgetClass;
};

#undef UE_API
