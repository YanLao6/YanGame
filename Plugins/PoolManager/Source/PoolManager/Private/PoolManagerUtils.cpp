// Copyright (c) Yevhenii Selivanov

#include "PoolManagerUtils.h"

// PoolManager
#include "Data/SpawnRequest.h"

// UE
#include "GameFeatureData.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PoolManagerUtils)

// 创建池对象的生成请求
FSpawnRequest UPoolManagerUtils::MakeSpawnRequest(TSubclassOf<UObject> ObjectClass, const FTransform& Transform, ESpawnRequestPriority Priority /* = ESpawnRequestPriority::Normal*/)
{
	if (!ensureMsgf(ObjectClass, TEXT("ASSERT: [%i] %hs:\n'ObjectClass' is null, can't create spawn request!"), __LINE__, __FUNCTION__))
	{
		return FSpawnRequest();
	}

	FSpawnRequest Request(ObjectClass);
	Request.Transform = Transform;
	Request.Priority = Priority;
	return Request;
}

// 给定类是否属于 GameFeatureData 所标识的 game feature 插件
bool UPoolManagerUtils::IsPoolInGameFeaturePlugin(const UClass* ObjectClass, const UGameFeatureData* GameFeatureData)
{
	if (!ObjectClass || !GameFeatureData)
	{
		return false;
	}

	// 从 GameFeatureData 所在包名中提取插件名
	const FString GameFeaturePackageName = GetNameSafe(GameFeatureData->GetOutermost());
	const int32 PluginSlashIdx = GameFeaturePackageName.Find(TEXT("/"), ESearchCase::CaseSensitive, ESearchDir::FromStart, 1);
	if (PluginSlashIdx == INDEX_NONE)
	{
		return false;
	}
	const FString PluginName = GameFeaturePackageName.Mid(1, PluginSlashIdx - 1);

	// 从类所在包名中解析出模块名
	const FString ClassPackageName = GetNameSafe(ObjectClass->GetOutermost());
	static const FString ScriptPrefix = TEXT("/Script/");
	FString ModuleName;
	if (ClassPackageName.StartsWith(ScriptPrefix))
	{
		// C++ 类：/Script/ModuleName -> ModuleName
		ModuleName = ClassPackageName.RightChop(ScriptPrefix.Len());
	}
	else
	{
		// 蓝图类：/ModuleName/... -> ModuleName
		const int32 ClassSlashIdx = ClassPackageName.Find(TEXT("/"), ESearchCase::CaseSensitive, ESearchDir::FromStart, 1);
		if (ClassSlashIdx != INDEX_NONE)
		{
			ModuleName = ClassPackageName.Mid(1, ClassSlashIdx - 1);
		}
	}

	return !PluginName.IsEmpty()
	       && !ModuleName.IsEmpty()
	       && ModuleName.StartsWith(PluginName);
}
