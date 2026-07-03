// Copyright (c) 2026 LazyDevelopers <lazydeveloper24@gmail.com>. All rights reserved.
// This plugin is distributed under the Fab Standard License.
//
// This product was independently developed by us while participating in the Epic Project, a developer-support
// program of the KRAFTON JUNGLE GameTech Lab. All rights, title, and interest in and to the product are exclusively
// vested in us. Krafton, Inc. was not involved in its development and distribution and disclaims all representations
// and warranties, express or implied, and assumes no responsibility or liability for any consequences arising from
// the use of this product.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Materials/MaterialInterface.h"
#include "Components/DestructionTypes.h"
#include "ImpactProfileDataAsset.generated.h"
 
USTRUCT(BlueprintType)
struct REALTIMEDESTRUCTION_API FImpactProfileConfig
{
	GENERATED_BODY()
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Decal")
	FString VariantName;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Decal")
	TObjectPtr<UMaterialInterface> DecalMaterial = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Decal")
	FVector DecalSize = FVector(1.0f, 10.0f, 10.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Decal")
	FVector LocationOffset = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Decal")
	FRotator RotationOffset = FRotator::ZeroRotator;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Decal")
	bool bRandomDecalRotation = true;

	// Tool 形状
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Decal")
	EDestructionToolShape ToolShape = EDestructionToolShape::Cylinder;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Decal")
	float CylinderRadius = 10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Decal")
	float CylinderHeight = 20.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Decal")
	float SphereRadius = 10.0f;

	// 有效性检查：Decal 材质不为空时才视为有效
	bool IsValid() const { return DecalMaterial != nullptr; }
};

USTRUCT(BlueprintType)
struct REALTIMEDESTRUCTION_API FImpactProfileConfigArray
{
	GENERATED_BODY()

	/** 该 Surface 可用的 Impact Profile 列表 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ImpactProfile")
	TArray<FImpactProfileConfig> Configs;

	/** 随机选取一个 Config */
	const FImpactProfileConfig* GetRandom() const
	{
		if (Configs.Num() == 0)
		{
			return nullptr;
		}

		if (Configs.Num() == 1)
		{
			return &Configs[0];
		}

		int32 Index;
		Index = FMath::RandRange(0, Configs.Num() - 1);

		return &Configs[Index]; 
	}

	/** 返回 Config 数量 */
	int32 Num() const { return Configs.Num(); }

	/** 有效性检查：列表非空且第一个元素有效 */
	bool IsValid() const { return (Configs.Num() > 0) && (Configs[0].IsValid()); }
};

USTRUCT(BlueprintType)
struct REALTIMEDESTRUCTION_API FProjectileImpactConfig
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ImpactProfile")
	FName ConfigID = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ImpactProfile")
	TMap<FName,	FImpactProfileConfigArray> SurfaceConfigs;
};

UCLASS(ClassGroup = (RealtimeDestruction), BlueprintType)
class REALTIMEDESTRUCTION_API UImpactProfileDataAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
#if WITH_EDITOR
	//~Begin UObject Interface
	virtual void PreEditChange(FProperty* PropertyAboutToChange) override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	//~End UObject Interface

private:
	// 修改前临时缓存 ConfigID，用于 PostEdit 期间的键名同步
	FName CachedConfigIDBeforeEdit;
#endif

public:  
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Decal")
	FName ConfigID = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ImpactProfile")
	TMap<FName,	FImpactProfileConfigArray> SurfaceConfigs;

public:
	UFUNCTION(BlueprintCallable, Category = "ImpactProfile")
	bool GetConfig( FName SurfaceType, int32 VariantIndex, FImpactProfileConfig& OutConfig ) const;

	UFUNCTION(BlueprintCallable, Category = "ImpactProfile")
	bool GetConfigRandom( FName SurfaceType, FImpactProfileConfig& OutConfig ) const;

	/** 返回当前持有的 Surface Key 数量 */
	UFUNCTION(BlueprintCallable, Category = "ImpactProfile")
	int32 GetSurfaceConfigCount() { return SurfaceConfigs.Num(); }
	

#if WITH_EDITORONLY_DATA
	UPROPERTY()
	FName CurrentEditingKey = NAME_None;
	
	UPROPERTY()
	FVector ToolShapeLocationInEditor = FVector::ZeroVector;
	
	UPROPERTY()
	FRotator ToolShapeRotationInEditor = FRotator::ZeroRotator;

	UPROPERTY()
	float SphereRadiusInEditor = 10.0f;

	UPROPERTY()
	float CylinderRadiusInEditor = 10.0f;
	
	UPROPERTY()
	float CylinderHeightInEditor = 10.0f;

	UPROPERTY()
	TSoftObjectPtr<UStaticMesh> PreviewMeshInEditor = nullptr;

	UPROPERTY()
	FVector PreviewMeshLocationInEditor = FVector::ZeroVector;

	UPROPERTY()
	FRotator PreviewMeshRotationInEditor = FRotator::ZeroRotator;
	
	UPROPERTY()
	FVector PreviewMeshScaleInEditor = FVector::OneVector;
	
#endif
};
