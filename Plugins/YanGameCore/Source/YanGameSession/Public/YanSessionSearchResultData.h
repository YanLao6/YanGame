// Copyright YanLao.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "YanSessionSearchResultData.generated.h"

#define UE_API YANGAMESESSION_API

class UCommonSession_SearchResult;

/**
 * 封装单条 Session 搜索结果，供 UListView 等 UI 控件用作 ListItem。
 * 提供格式化好的文本 Getter，方便 AngelScript / Blueprint 直接绑定。
 */
UCLASS(MinimalAPI, BlueprintType)
class UYanSessionSearchResultData : public UObject
{
	GENERATED_BODY()

public:
	/** 原始搜索结果，由 UCommonSessionSubsystem::FindSessions 返回 */
	UPROPERTY(BlueprintReadOnly, Category = "Session")
	TObjectPtr<UCommonSession_SearchResult> SearchResult;

	/** 返回 "当前人数 / 最大人数"，例如 "2 / 4" */
	UFUNCTION(BlueprintPure, Category = "Session")
	UE_API FText GetPlayerCountText() const;

	/** 返回延迟字符串，例如 "32 ms"；不可达时返回 "N/A" */
	UFUNCTION(BlueprintPure, Category = "Session")
	UE_API FText GetPingText() const;

	/** 返回 Session 内部描述（调试 / 列表展示用） */
	UFUNCTION(BlueprintPure, Category = "Session")
	UE_API FText GetDescriptionText() const;
};

#undef UE_API
