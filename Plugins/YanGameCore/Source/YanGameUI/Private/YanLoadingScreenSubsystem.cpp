// 加载屏子系统实现。

#include "YanLoadingScreenSubsystem.h"

#include "Blueprint/UserWidget.h"
#include "Engine/AssetManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(YanLoadingScreenSubsystem)

UYanLoadingScreenSubsystem::UYanLoadingScreenSubsystem()
{
}

void UYanLoadingScreenSubsystem::SetLoadingScreenContentWidget(TSubclassOf<UUserWidget> NewWidgetClass)
{
	if (LoadingScreenWidgetClass != NewWidgetClass)
	{
		LoadingScreenWidgetClass = NewWidgetClass;

		OnLoadingScreenWidgetChanged.Broadcast(LoadingScreenWidgetClass);
	}
}

TSubclassOf<UUserWidget> UYanLoadingScreenSubsystem::GetLoadingScreenContentWidget() const
{
	return LoadingScreenWidgetClass;
}

void UYanLoadingScreenSubsystem::PrepareLoadingScreenForExperience(TSoftClassPtr<UUserWidget> ExperienceLoadingScreenWidget)
{
	if (ExperienceLoadingScreenWidget.IsNull())
	{
		SetLoadingScreenContentWidget(nullptr);
		return;
	}

	UAssetManager::GetStreamableManager().RequestAsyncLoad(
		ExperienceLoadingScreenWidget.ToSoftObjectPath(),
		FStreamableDelegate::CreateWeakLambda(this, [this, ExperienceLoadingScreenWidget]
		{
			SetLoadingScreenContentWidget(ExperienceLoadingScreenWidget.Get());
		}));
}
