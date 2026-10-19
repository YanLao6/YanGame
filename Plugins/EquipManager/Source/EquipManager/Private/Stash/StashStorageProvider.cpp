// 仓库存储提供者实现。

#include "Stash/StashStorageProvider.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(StashStorageProvider)

void UStashStorageProvider::LoadStash(const ULocalPlayer* LocalPlayer, FStashLoadComplete OnComplete)
{
	// 基类不提供任何存储，回调仍须触发，否则等待方会永远停在加载中。
	OnComplete.ExecuteIfBound(false, FStashSnapshot());
}

void UStashStorageProvider::SaveStash(const ULocalPlayer* LocalPlayer, const FStashSnapshot& Snapshot, FStashSaveComplete OnComplete)
{
	OnComplete.ExecuteIfBound(false);
}

bool UStashStorageProvider::IsAuthoritative() const
{
	return false;
}
