// Copyright (c) Yevhenii Selivanov.

#pragma once

#include "Modules/ModuleInterface.h"

class POOLMANAGEREDITOR_API FPoolManagerEditorModule : public IModuleInterface
{
public:
    //~Begin IModuleInterface
    /**
     * 在模块 DLL 加载完毕、模块对象创建之后立即调用。
     * 在此加载依赖模块，可保证它们在 ShutdownModule 期间仍然可用。
     */
    virtual void StartupModule() override;

    /**
    * 在模块被卸载之前、模块对象销毁之前调用。
    * 正常关闭流程中，其调用顺序与各模块完成 StartupModule 的顺序相反。
    * 因此只要模块在 StartupModule() 中引用了依赖模块，
    * 就同样可以在 ShutdownModule() 中安全引用这些依赖。
    */
    virtual void ShutdownModule() override;
    //~End IModuleInterface
};
