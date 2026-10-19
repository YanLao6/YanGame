// Copyright (c) Yevhenii Selivanov

#include "Data/PoolManagerSettings.h"

// Pool Manager
#include "Factories/PoolFactory_UObject.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PoolManagerSettings)

// 返回 Pool Manager 将要使用的全部 Pool Factory
void UPoolManagerSettings::GetPoolFactories(TArray<UClass*>& OutBlueprintPoolFactories) const
{
	if (!OutBlueprintPoolFactories.IsEmpty())
	{
		OutBlueprintPoolFactories.Empty();
	}

	for (const TSoftClassPtr<UPoolFactory_UObject>& It : PoolFactories)
	{
		if (UClass* PoolFactoryClass = It.LoadSynchronous())
		{
			OutBlueprintPoolFactories.Emplace(PoolFactoryClass);
		}
	}
}
