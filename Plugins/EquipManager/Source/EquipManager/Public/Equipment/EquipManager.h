// EquipManager 模块声明。

#pragma once

#include "Modules/ModuleManager.h"

/**
 * EquipManager 运行时模块。
 *
 * 负责接收插件模块的加载与卸载回调，并为后续运行时注册逻辑预留统一入口。
 */
class FEquipManagerModule : public IModuleInterface
{
public:
	//~Begin IModuleInterface Interface
	// 模块加载完成后调用，可在此注册运行时系统。
	virtual void StartupModule() override;
	// 模块卸载前调用，可在此释放已注册资源。
	virtual void ShutdownModule() override;
	//~End IModuleInterface Interface
};
