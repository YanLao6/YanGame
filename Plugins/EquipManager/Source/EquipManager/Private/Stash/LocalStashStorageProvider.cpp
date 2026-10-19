// 本地存档形式的仓库存储实现。

#include "Stash/LocalStashStorageProvider.h"

#include "Engine/LocalPlayer.h"
#include "ModularGameplayAbilitiesLogChannels.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LocalStashStorageProvider)

namespace
{
	// 仓库存档的槽位名，与设置存档分开以免互相覆盖。
	const TCHAR* StashSaveSlotName = TEXT("PlayerStash");
}

//~Begin ULocalPlayerSaveGame Interface
int32 UStashSaveGame::GetLatestDataVersion() const
{
	return 1;
}
//~End ULocalPlayerSaveGame Interface

//~Begin UStashStorageProvider Interface
void ULocalStashStorageProvider::LoadStash(const ULocalPlayer* LocalPlayer, FStashLoadComplete OnComplete)
{
	if (LocalPlayer == nullptr)
	{
		OnComplete.ExecuteIfBound(false, FStashSnapshot());
		return;
	}

	TWeakObjectPtr<ULocalStashStorageProvider> WeakThis(this);

	const bool bScheduled = ULocalPlayerSaveGame::AsyncLoadOrCreateSaveGameForLocalPlayer(
		UStashSaveGame::StaticClass(),
		LocalPlayer,
		StashSaveSlotName,
		FOnLocalPlayerSaveGameLoadedNative::CreateLambda(
			[WeakThis, OnComplete](ULocalPlayerSaveGame* LoadedGame)
			{
				ULocalStashStorageProvider* Provider = WeakThis.Get();
				UStashSaveGame*             StashGame = Cast<UStashSaveGame>(LoadedGame);

				if ((Provider == nullptr) || (StashGame == nullptr))
				{
					OnComplete.ExecuteIfBound(false, FStashSnapshot());
					return;
				}

				// 存档不存在时引擎会新建一个空实例，对仓库而言等同于「还没有任何东西」，属正常结果。
				Provider->LoadedSaveGame = StashGame;
				OnComplete.ExecuteIfBound(true, StashGame->Snapshot);
			}));

	if (!bScheduled)
	{
		UE_LOG(LogModularGameplayAbilities, Warning, TEXT("仓库存档读取未能发起，玩家 [%s]。"), *GetNameSafe(LocalPlayer));
		OnComplete.ExecuteIfBound(false, FStashSnapshot());
	}
}

void ULocalStashStorageProvider::SaveStash(const ULocalPlayer* LocalPlayer, const FStashSnapshot& Snapshot, FStashSaveComplete OnComplete)
{
	if (LoadedSaveGame == nullptr)
	{
		// 读取建立了存档对象与玩家的关联，没有这层关联就无从确定写往哪个槽位。
		UE_LOG(LogModularGameplayAbilities, Warning, TEXT("仓库尚未读取便请求写入，本次写入被丢弃。"));
		OnComplete.ExecuteIfBound(false);
		return;
	}

	LoadedSaveGame->Snapshot = Snapshot;

	const bool bRequested = LoadedSaveGame->AsyncSaveGameToSlotForLocalPlayer();
	OnComplete.ExecuteIfBound(bRequested);
}
//~End UStashStorageProvider Interface
