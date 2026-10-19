// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"

#include "ModularTeamStatics.generated.h"

#define UE_API MODULARTEAMS_API

class UModularTeamDisplayAsset;
class UTexture;

/** 队伍信息的蓝图访问入口。 */
UCLASS(MinimalAPI)
class UModularTeamStatics : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** 查询对象所属队伍及其展示资产，不属于任何队伍时 TeamId 为 INDEX_NONE。 */
	UFUNCTION(BlueprintCallable, Category = Teams, meta = (Keywords = "GetTeamFromObject", DefaultToSelf = "Agent", AdvancedDisplay = "bLogIfNotSet"))
	static UE_API void FindTeamFromObject(const UObject* Agent, bool& bIsPartOfTeam, int32& TeamId, UModularTeamDisplayAsset*& DisplayAsset, bool bLogIfNotSet = false);

	/** 按队伍编号取展示资产。 */
	UFUNCTION(BlueprintCallable, Category = Teams, meta = (WorldContext = "WorldContextObject"))
	static UE_API UModularTeamDisplayAsset* GetTeamDisplayAsset(const UObject* WorldContextObject, int32 TeamId);

	/** 读取标量参数，缺省时返回 DefaultValue。 */
	UFUNCTION(BlueprintCallable, Category = Teams)
	static UE_API float GetTeamScalarWithFallback(UModularTeamDisplayAsset* DisplayAsset, FName ParameterName, float DefaultValue);

	/** 读取颜色参数，缺省时返回 DefaultValue。 */
	UFUNCTION(BlueprintCallable, Category = Teams)
	static UE_API FLinearColor GetTeamColorWithFallback(UModularTeamDisplayAsset* DisplayAsset, FName ParameterName, FLinearColor DefaultValue);

	/** 读取纹理参数，缺省时返回 DefaultValue。 */
	UFUNCTION(BlueprintCallable, Category = Teams)
	static UE_API UTexture* GetTeamTextureWithFallback(UModularTeamDisplayAsset* DisplayAsset, FName ParameterName, UTexture* DefaultValue);
};

#undef UE_API
