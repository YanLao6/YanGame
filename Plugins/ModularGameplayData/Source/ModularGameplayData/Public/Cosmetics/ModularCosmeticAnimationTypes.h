// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GameplayTagContainer.h"
#include "Templates/SubclassOf.h"

#include "ModularCosmeticAnimationTypes.generated.h"

#define UE_API MODULARGAMEPLAYDATA_API

class UAnimInstance;
class UPhysicsAsset;
class USkeletalMesh;

//////////////////////////////////////////////////////////////////////

USTRUCT(BlueprintType)
struct FModularAnimLayerSelectionEntry
{
	GENERATED_BODY()

	/** Tag 匹配时应用的 AnimInstance Layer（TSubclassOf<UAnimInstance>）。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSubclassOf<UAnimInstance> Layer;

	/** 必须全部具备的 Cosmetic GameplayTag（HasAll）。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(Categories="Cosmetic"))
	FGameplayTagContainer RequiredTags;
};

USTRUCT(BlueprintType)
struct FModularAnimLayerSelectionSet
{
	GENERATED_BODY()
		
	/** 规则列表：按数组顺序取首个匹配项。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(TitleProperty=Layer))
	TArray<FModularAnimLayerSelectionEntry> LayerRules;

	/** 无任何规则匹配时的默认 AnimInstance Class。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSubclassOf<UAnimInstance> DefaultLayer;

	/** 根据当前 CosmeticTags 选择最合适的 Layer Class。 */
	UE_API TSubclassOf<UAnimInstance> SelectBestLayer(const FGameplayTagContainer& CosmeticTags) const;
};

//////////////////////////////////////////////////////////////////////

USTRUCT(BlueprintType)
struct FModularAnimBodyStyleSelectionEntry
{
	GENERATED_BODY()

	/** Tag 匹配时使用的 USkeletalMesh。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TObjectPtr<USkeletalMesh> Mesh = nullptr;

	/** 必须全部具备的 Cosmetic GameplayTag。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(Categories="Cosmetic"))
	FGameplayTagContainer RequiredTags;
};

USTRUCT(BlueprintType)
struct FModularAnimBodyStyleSelectionSet
{
	GENERATED_BODY()
		
	/** Mesh 规则表：顺序优先，首个 HasAll 命中即采用。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(TitleProperty=Mesh))
	TArray<FModularAnimBodyStyleSelectionEntry> MeshRules;

	/** 无规则匹配时的默认 SkeletalMesh。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TObjectPtr<USkeletalMesh> DefaultMesh = nullptr;

	/** 若指定，则强制使用该 PhysicsAsset（与 BodyStyle 解耦时使用）。 */
	UPROPERTY(EditAnywhere)
	TObjectPtr<UPhysicsAsset> ForcedPhysicsAsset = nullptr;

	/** 根据 CosmeticTags 选择 BodyStyle 对应的 USkeletalMesh。 */
	USkeletalMesh* SelectBestBodyStyle(const FGameplayTagContainer& CosmeticTags) const;
};

#undef UE_API
