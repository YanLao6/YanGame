// Copyright YanLao.

#include "YanSessionSearchResultData.h"
#include "CommonSessionSubsystem.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(YanSessionSearchResultData)

FText UYanSessionSearchResultData::GetPlayerCountText() const
{
	if (!SearchResult)
	{
		return FText::GetEmpty();
	}

	const int32 Max = SearchResult->GetMaxPublicConnections();
	const int32 Open = SearchResult->GetNumOpenPublicConnections();
	const int32 Current = FMath::Max(0, Max - Open);

	return FText::FromString(FString::Printf(TEXT("%d / %d"), Current, Max));
}

FText UYanSessionSearchResultData::GetPingText() const
{
	if (!SearchResult)
	{
		return FText::GetEmpty();
	}

	const int32 Ping = SearchResult->GetPingInMs();

	// MAX_QUERY_PING (= 9999) 表示不可达
	if (Ping >= 9999)
	{
		return FText::FromString(TEXT("N/A"));
	}

	return FText::FromString(FString::Printf(TEXT("%d ms"), Ping));
}

FText UYanSessionSearchResultData::GetDescriptionText() const
{
	if (!SearchResult)
	{
		return FText::GetEmpty();
	}

	return FText::FromString(SearchResult->GetDescription());
}
