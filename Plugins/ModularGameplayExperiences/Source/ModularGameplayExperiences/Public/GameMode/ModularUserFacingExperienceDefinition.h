// Copyright Chronicler.

#pragma once

#include "CommonSessionSubsystem.h"
#include "Engine/DataAsset.h"

#include "ModularUserFacingExperienceDefinition.generated.h"

#define UE_API MODULARGAMEPLAYEXPERIENCES_API

/**
 * 面向玩家 UI 的 Experience 描述：地图、GameplayExperience、会话参数与展示文案。
 */
UCLASS(MinimalAPI, BlueprintType)
class UModularUserFacingExperienceDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** 要加载的地图 PrimaryAssetId。 */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category=Experience, meta=(AllowedTypes="Map"))
	FPrimaryAssetId MapID;

	/** 要加载的 ModularExperienceDefinition。 */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category=Experience, meta=(AllowedTypes="ModularExperienceDefinition"))
	FPrimaryAssetId ExperienceID;

	/** 追加到 URL 的键值参数。 */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category=Experience)
	TMap<FString, FString> ExtraArgs;

	/** 主标题（磁贴）。 */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category=Experience)
	FText TileTitle;

	/** 副标题。 */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category=Experience)
	FText TileSubTitle;

	/** 完整描述。 */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category=Experience)
	FText TileDescription;

	/** 磁贴图标。 */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category=Experience)
	TObjectPtr<UTexture2D> TileIcon;

	/** 加载进/退出 Experience 时显示的 LoadingScreen Widget。 */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category=LoadingScreen)
	TSoftClassPtr<UUserWidget> LoadingScreenWidget;

	/** 是否为默认 Experience（QuickPlay / UI 排序优先）。 */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category=Experience)
	bool bIsDefaultExperience = false;

	/** 是否在前端 Experience 列表展示。 */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category=Experience)
	bool bShowInFrontEnd = true;

	/** 是否录制 Replay。 */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category=Experience)
	bool bRecordReplay = false;

	/** 会话最大玩家数。 */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category=Experience)
	int32 MaxPlayerCount = 16;

public:
	/** 构造 UCommonSession_HostSessionRequest，用于真正发起 Hosting。 */
	UFUNCTION(BlueprintCallable, BlueprintPure=false)
	UE_API UCommonSession_HostSessionRequest* CreateHostingRequest(const UObject* WorldContextObject) const;
};

#undef UE_API
