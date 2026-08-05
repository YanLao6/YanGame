#pragma once

#include "Engine/DataAsset.h"

#include "HeroCatalog.generated.h"

#define UE_API HEROASSIGNMENTSYSTEM_API

class UModularPawnData;
class UTexture2D;

/** 单个可选角色：游玩数据与选人界面所需的展示信息。 */
USTRUCT(BlueprintType)
struct FHeroEntry
{
	GENERATED_BODY()

	/** 该角色的游玩数据，决定 Pawn 类、技能集与输入配置。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Hero")
	TObjectPtr<UModularPawnData> PawnData;

	/** 选人界面显示的名称。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Hero")
	FText DisplayName;

	/** 选人界面显示的头像。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Hero")
	TObjectPtr<UTexture2D> Icon;
};

/**
 * 角色目录。
 *
 * 大厅选人界面与对战角色决议共用同一份候选来源。两处若各自配置，玩家选中的角色
 * 可能在对战关卡查不到而被静默替换为兜底角色，该故障不报错且难以察觉。
 *
 * 条目使用硬引用，随目录一同加载，使角色决议无需同步加载资产。
 */
UCLASS(MinimalAPI, BlueprintType, Const, Meta = (DisplayName = "Hero Catalog"))
class UHeroCatalog : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** 全部可选角色。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Hero")
	TArray<FHeroEntry> Heroes;

	/**
	 * 取指定条目的角色 Id。
	 *
	 * GetPrimaryAssetId 非 UFUNCTION，脚本层无法直接构造 Id，选人界面经此获取后
	 * 回传给 UHeroSelectionComponent::RequestSelectHero。索引越界返回无效值。
	 */
	UFUNCTION(BlueprintPure, Category = "Hero")
	UE_API FPrimaryAssetId GetHeroIdAt(int32 Index) const;

	/** 查找角色 Id 在目录中的索引，不存在时返回 INDEX_NONE。 */
	UFUNCTION(BlueprintPure, Category = "Hero")
	UE_API int32 IndexOfHero(FPrimaryAssetId HeroId) const;

	/** 按 PawnData 的 PrimaryAssetId 查找角色数据，不在目录内返回 nullptr。 */
	UE_API const UModularPawnData* FindPawnData(const FPrimaryAssetId& HeroId) const;

	/** 目录中是否不存在任何有效条目。 */
	UE_API bool IsEmpty() const;
};

#undef UE_API
