// Copyright Chronicler.

#pragma once

#include "Containers/Ticker.h"
#include "GameUIManagerSubsystem.h"

#include "ModularUIManagerSubsystem.generated.h"

class FSubsystemCollectionBase;
class UObject;

/**
 * UI 管理子系统扩展：按 HUD bShowHUD 同步 PrimaryGameLayout 可见性。
 */
UCLASS(MinimalAPI)
class UModularUIManagerSubsystem : public UGameUIManagerSubsystem
{
	GENERATED_BODY()

public:

	UModularUIManagerSubsystem();

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

private:
	// FTSTicker：每帧同步 RootLayout 与 HUD 显隐
	bool Tick(float DeltaTime);
	// bShowHUD == false 时折叠 RootLayout，避免挡住关卡但保留输入策略
	void SyncRootLayoutVisibilityToShowHUD();
	
	FTSTicker::FDelegateHandle TickHandle;
};
