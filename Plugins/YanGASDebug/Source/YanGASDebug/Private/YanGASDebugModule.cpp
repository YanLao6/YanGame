// Copyright YanGame.

#include "YanGASDebug/Public/YanGASDebugModule.h"

#include "GameFramework/CheatManager.h"
#include "YanGASDebug/Public/YanGASCheatExtension.h"

#define LOCTEXT_NAMESPACE "FYanGASDebugModule"

void FYanGASDebugModule::StartupModule()
{
#if !UE_BUILD_SHIPPING
	// 为每个已存在及后续生成的 CheatManager 挂载调试扩展。
	CheatManagerCreatedHandle = UCheatManager::RegisterForOnCheatManagerCreated(
		FOnCheatManagerCreated::FDelegate::CreateLambda(
			[](UCheatManager* CheatManager)
			{
				CheatManager->AddCheatManagerExtension(NewObject<UYanGASCheatExtension>(CheatManager));
			}));
#endif
}

void FYanGASDebugModule::ShutdownModule()
{
#if !UE_BUILD_SHIPPING
	if (CheatManagerCreatedHandle.IsValid())
	{
		UCheatManager::UnregisterFromOnCheatManagerCreated(CheatManagerCreatedHandle);
		CheatManagerCreatedHandle.Reset();
	}
#endif
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FYanGASDebugModule, YanGASDebug)
