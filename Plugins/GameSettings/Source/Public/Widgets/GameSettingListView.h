// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/ListView.h"

#include "GameSettingListView.generated.h"

#define UE_API GAMESETTINGS_API

class STableViewBase;

class UGameSettingCollection;
class ULocalPlayer;
class UGameSettingVisualData;

/**
 * 游戏设置列表。每个 entry widget 都必须派生自 GameSettingListEntryBase。
 */
UCLASS(MinimalAPI, meta = (EntryClass = GameSettingListEntryBase))
class UGameSettingListView : public UListView
{
	GENERATED_BODY()

public:
	UE_API UGameSettingListView(const FObjectInitializer& ObjectInitializer);

	UE_API void AddNameOverride(const FName& DevName, const FText& OverrideName);

#if WITH_EDITOR
	UE_API virtual void ValidateCompiledDefaults(IWidgetCompilerLog& InCompileLog) const override;
#endif

protected:
	UE_API virtual UUserWidget& OnGenerateEntryWidgetInternal(UObject* Item, TSubclassOf<UUserWidget> DesiredEntryClass, const TSharedRef<STableViewBase>& OwnerTable) override;
	UE_API virtual bool OnIsSelectableOrNavigableInternal(UObject* SelectedItem) override;

protected:
	UPROPERTY(EditAnywhere)
	TObjectPtr<UGameSettingVisualData> VisualData;

private:
	TMap<FName, FText> NameOverrides;
};

#undef UE_API
