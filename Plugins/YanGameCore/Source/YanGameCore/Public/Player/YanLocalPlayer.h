// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "CommonLocalPlayer.h"
#include "YanLocalPlayer.generated.h"

#define UE_API YANGAMECORE_API

class UYanSettingsLocal;
class UYanSettingsShared;

/**
 *
 */
UCLASS(MinimalAPI)
class UYanLocalPlayer : public UCommonLocalPlayer
{
	GENERATED_BODY()

public:
	UE_API UYanLocalPlayer();

	// 设置项的属性路径以 ULocalPlayer 为根解析，下面两个访问器即路径的第一跳。
	// 数据本身分别由 UYanSettingsLocal 与 UYanSettingsSubsystem 持有，此处不存储状态。
	// 重命名会使设置树的取值失效，改动需同步 YanGameSettingRegistry.h 中的路径常量。

	/** 本机画面设置，进程启动时即从配置文件读入，任何时候都有效 */
	UFUNCTION()
	UE_API UYanSettingsLocal* GetLocalSettings() const;

	/** 玩家共享设置，经存档系统读取，登录完成前可能拿到的是临时对象 */
	UFUNCTION()
	UE_API UYanSettingsShared* GetSharedSettings() const;
};

#undef UE_API
