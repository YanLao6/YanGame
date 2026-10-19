// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Subsystems/LocalPlayerSubsystem.h"

#include "YanSettingsSubsystem.generated.h"

#define UE_API YANGAMESETTING_API

class UYanSettingsShared;

/**
 * 共享设置的持有者。
 * 设置对象经存档系统按玩家加载，放在本地玩家子系统上，
 * 使输入 Modifier、设置树等任意模块都能取用，而无需依赖具体的 LocalPlayer 子类。
 */
UCLASS(MinimalAPI)
class UYanSettingsSubsystem : public ULocalPlayerSubsystem
{
	GENERATED_BODY()

public:
	//~Begin USubsystem Interface
	UE_API virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	//~End USubsystem Interface

	/** 便捷取用；本地玩家为空或子系统缺失时返回 nullptr */
	static UE_API UYanSettingsShared* GetSharedSettingsFor(const ULocalPlayer* LocalPlayer);

	/** 取共享设置，尚未加载时按平台能力同步加载或先给出临时对象 */
	UE_API UYanSettingsShared* GetSharedSettings() const;

	/** 发起异步加载，完成后替换当前实例 */
	UE_API void LoadSharedSettingsFromDisk(bool bForceLoad = false);

private:
	void OnSharedSettingsLoaded(UYanSettingsShared* LoadedOrCreatedSettings);

	UPROPERTY(Transient)
	mutable TObjectPtr<UYanSettingsShared> SharedSettings;

	// 记录加载时的账号，账号切换后需要重新读取
	FUniqueNetIdRepl NetIdForSharedSettings;
};

#undef UE_API
