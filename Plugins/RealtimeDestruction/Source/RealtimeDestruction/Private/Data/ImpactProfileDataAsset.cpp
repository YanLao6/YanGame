// Copyright (c) 2026 LazyDevelopers <lazydeveloper24@gmail.com>. All rights reserved.
// This plugin is distributed under the Fab Standard License.
//
// This product was independently developed by us while participating in the Epic Project, a developer-support
// program of the KRAFTON JUNGLE GameTech Lab. All rights, title, and interest in and to the product are exclusively
// vested in us. Krafton, Inc. was not involved in its development and distribution and disclaims all representations
// and warranties, express or implied, and assumes no responsibility or liability for any consequences arising from
// the use of this product.

#include "Data/ImpactProfileDataAsset.h"

#if WITH_EDITOR

#include "Settings/RDMSetting.h"
void UImpactProfileDataAsset::PreEditChange(FProperty* PropertyAboutToChange)
{
	Super::PreEditChange(PropertyAboutToChange);

	// 在更改Config ID之前保存当前值
	if (PropertyAboutToChange &&
		PropertyAboutToChange->GetFName() == GET_MEMBER_NAME_CHECKED(UImpactProfileDataAsset, ConfigID))
	{
		CachedConfigIDBeforeEdit = ConfigID;
	}
}

void UImpactProfileDataAsset::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (PropertyChangedEvent.GetPropertyName() == GET_MEMBER_NAME_CHECKED(UImpactProfileDataAsset, ConfigID))
	{
		// 检查值是否确实已更改
		if (!CachedConfigIDBeforeEdit.IsNone() && CachedConfigIDBeforeEdit != ConfigID)
		{
			// 更新Project Settings
			if (URDMSetting* Settings = URDMSetting::Get())
			{
				Settings->UpdateEntryConfigID(CachedConfigIDBeforeEdit, ConfigID);
			}
		}

		CachedConfigIDBeforeEdit = NAME_None; 
	}
}

#endif

bool UImpactProfileDataAsset::GetConfig( FName SurfaceType, int32 VariantIndex,
	FImpactProfileConfig& OutConfig) const
{ 

	const FImpactProfileConfigArray* FoundArray = SurfaceConfigs.Find(SurfaceType);
	
	if (!FoundArray && SurfaceType != "Default")
	{
		FoundArray = SurfaceConfigs.Find("Default");
	}

	if (!FoundArray || FoundArray->Configs.Num() == 0)
	{
		return false;
	}

	// 检查VariantIndex范围
	int32 SafeIndex = FMath::Clamp(VariantIndex, 0, FoundArray->Configs.Num() - 1);
	OutConfig = FoundArray->Configs[SafeIndex];
	return true; 
}

bool UImpactProfileDataAsset::GetConfigRandom( FName SurfaceType, FImpactProfileConfig& OutConfig) const
{ 
	// 通过SurfaceType查找DecalConfig
	if ( const FImpactProfileConfigArray* FoundArray = SurfaceConfigs.Find(SurfaceType))
	{
		const FImpactProfileConfig* Selected = FoundArray->GetRandom();
		if (Selected)
		{
			OutConfig = *Selected;
			return true;
		}
	}

	// 如果找不到DecalConfig，请尝试分配默认值
	if (SurfaceType != "Default")
	{
		if (const FImpactProfileConfigArray* DefaultConfig = SurfaceConfigs.Find("Default"))
		{
			const FImpactProfileConfig* Selected = DefaultConfig->GetRandom();
			if (Selected)
			{ 
				OutConfig = *Selected;
				return true;
			}
		}
	}

	return false;
} 
