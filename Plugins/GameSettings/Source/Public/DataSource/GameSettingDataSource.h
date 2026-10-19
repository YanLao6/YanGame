// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Delegates/Delegate.h"

class ULocalPlayer;

//--------------------------------------
// FGameSettingDataSource
//--------------------------------------

class FGameSettingDataSource : public TSharedFromThis<FGameSettingDataSource>
{
public:
	virtual ~FGameSettingDataSource() { }

	/**
	 * 部分设置的初始化是异步的；设置系统会等待全部设置就绪后再显示。
	 */
	virtual void Startup(ULocalPlayer* InLocalPlayer, FSimpleDelegate StartupCompleteCallback) { StartupCompleteCallback.ExecuteIfBound(); }

	virtual bool Resolve(ULocalPlayer* InContext) = 0;

	virtual FString GetValueAsString(ULocalPlayer* InContext) const = 0;

	virtual void SetValue(ULocalPlayer* InContext, const FString& Value) = 0;

	virtual FString ToString() const = 0;
};
