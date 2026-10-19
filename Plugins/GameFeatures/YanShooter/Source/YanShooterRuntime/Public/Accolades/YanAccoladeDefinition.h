// 战斗嘉奖定义声明。

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Engine/DataTable.h"
#include "GameplayTagContainer.h"
#include "YanAccoladeDefinition.generated.h"

class USoundBase;

/**
 * 战斗嘉奖的数据表行。
 *
 * 由 DataRegistry 按 Tag 名异步取用，描述一次嘉奖的文本、图标、音效与展示时长。
 */
USTRUCT(BlueprintType)
struct FYanAccoladeDefinitionRow : public FTableRowBase
{
	GENERATED_BODY()

public:
	/** 展示文本。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FText DisplayName;

	/** 展示时播放的音效。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TSoftObjectPtr<USoundBase> Sound;

	/** 展示图标。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(DisplayThumbnail="true", AllowedClasses="/Script/Engine.Texture,/Script/Engine.MaterialInterface,/Script/Engine.SlateTextureAtlasInterface", DisallowedClasses="/Script/MediaAssets.MediaTexture"))
	TSoftObjectPtr<UObject> Icon;

	/** 展示时长，单位为秒。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	float DisplayDuration = 1.0f;

	/** 展示位置，用于把嘉奖投递到对应的宿主控件。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FGameplayTag LocationTag;

	/** 本条嘉奖自身携带的 Tag。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FGameplayTagContainer AccoladeTags;

	/** 本条嘉奖出现时，需要顶掉的其他嘉奖所带的 Tag（例如三杀顶掉双杀）。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FGameplayTagContainer CancelAccoladesWithTag;
};

/**
 * 战斗嘉奖定义资产。
 */
UCLASS(MinimalAPI, BlueprintType)
class UYanAccoladeDefinition : public UDataAsset
{
	GENERATED_BODY()

public:
	/** 展示时播放的音效。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TObjectPtr<USoundBase> Sound;

	/** 展示图标。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(DisplayThumbnail="true", AllowedClasses="/Script/Engine.Texture,/Script/Engine.MaterialInterface,/Script/Engine.SlateTextureAtlasInterface", DisallowedClasses="/Script/MediaAssets.MediaTexture"))
	TObjectPtr<UObject> Icon;

	/** 本条嘉奖自身携带的 Tag。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FGameplayTagContainer AccoladeTags;

	/** 本条嘉奖出现时，需要顶掉的其他嘉奖所带的 Tag。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FGameplayTagContainer CancelAccoladesWithTag;
};
