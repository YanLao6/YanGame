// 本地存档形式的仓库存储声明。

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "Stash/StashStorageProvider.h"
#include "LocalStashStorageProvider.generated.h"

#define UE_API EQUIPMANAGER_API

/**
 * 仓库存档。
 *
 * 只承载一份快照；快照结构变化时递增数据版本，迁移在仓库层读取后进行。
 */
UCLASS(MinimalAPI)
class UStashSaveGame : public ULocalPlayerSaveGame
{
	GENERATED_BODY()

public:
	//~Begin ULocalPlayerSaveGame Interface
	UE_API virtual int32 GetLatestDataVersion() const override;
	//~End ULocalPlayerSaveGame Interface

	/** 存档中保存的仓库快照。 */
	UPROPERTY()
	FStashSnapshot Snapshot;
};

/**
 * 本地存档形式的仓库存储。
 *
 * 数据落在玩家本机，因而不具权威性：玩家可以改写存档，凭其表达的拥有权不可尽信。
 */
UCLASS(MinimalAPI)
class ULocalStashStorageProvider : public UStashStorageProvider
{
	GENERATED_BODY()

public:
	//~Begin UStashStorageProvider Interface
	UE_API virtual void LoadStash(const ULocalPlayer* LocalPlayer, FStashLoadComplete OnComplete) override;
	UE_API virtual void SaveStash(const ULocalPlayer* LocalPlayer, const FStashSnapshot& Snapshot, FStashSaveComplete OnComplete) override;
	//~End UStashStorageProvider Interface

private:
	// 已加载的存档对象；写入以对象为单位，故须留存而不能每次新建，
	// 新建的对象没有与玩家建立关联，写入会落到错误的槽位或直接失败。
	UPROPERTY(Transient)
	TObjectPtr<UStashSaveGame> LoadedSaveGame;
};

#undef UE_API
