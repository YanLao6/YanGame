// Copyright YanGame.

#pragma once

#include "Modules/ModuleInterface.h"
#include "Delegates/IDelegateInstance.h"

/**
 * YanGASDebug 模块。
 *
 * 启动时向 UCheatManager 注册委托，为每个生成的 CheatManager 挂载
 * UYanGASCheatExtension，从而无需修改 PlayerController 即可提供 GAS 调试命令。
 * 仅在非 Shipping 构建中执行注册。
 */
class FYanGASDebugModule : public IModuleInterface
{
public:
	//~Begin IModuleInterface
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	//~End IModuleInterface

private:
	// CheatManager 创建委托句柄，用于关闭模块时注销。
	FDelegateHandle CheatManagerCreatedHandle;
};
