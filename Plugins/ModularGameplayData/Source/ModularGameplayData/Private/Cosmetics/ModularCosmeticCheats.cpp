// Copyright Epic Games, Inc. All Rights Reserved.

#include "Cosmetics/ModularCosmeticCheats.h"

#include "AssetRegistry/ARFilter.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Cosmetics/ModularCharacterPartTypes.h"
#include "Cosmetics/ModularControllerComponent_CharacterParts.h"
#include "Engine/Blueprint.h"
#include "GameFramework/CheatManagerDefines.h"
#include "Misc/PackageName.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ModularCosmeticCheats)

namespace
{
	/** 在资产注册表中按短名查找蓝图生成类。 */
	UClass* FindBlueprintClassByShortName(const FString& TargetNameRaw, UClass* DesiredBaseClass)
	{
		FString TargetName = TargetNameRaw;
		TargetName.RemoveFromEnd(TEXT("_C"), ESearchCase::CaseSensitive);

		FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));

		FARFilter Filter;
		Filter.ClassPaths.Add(UBlueprint::StaticClass()->GetClassPathName());
		Filter.bRecursivePaths = true;

		TArray<FAssetData> BlueprintList;
		AssetRegistryModule.Get().GetAssets(Filter, BlueprintList);

		for (const FAssetData& AssetData : BlueprintList)
		{
			if ((AssetData.AssetName.ToString() != TargetName) && (AssetData.GetObjectPathString() != TargetName))
			{
				continue;
			}

			if (const UBlueprint* BP = Cast<UBlueprint>(AssetData.GetAsset()))
			{
				if (UClass* GeneratedClass = BP->GeneratedClass)
				{
					if (GeneratedClass->IsChildOf(DesiredBaseClass))
					{
						return GeneratedClass;
					}
				}
			}
		}

		return nullptr;
	}

	/**
	 * 按控制台输入的短名解析类，便于作弊指令直接写资产名。
	 * 先查已加载的原生类与资产，未命中再退到资产注册表扫描蓝图。
	 */
	UClass* FindClassByShortName(const FString& SearchToken, UClass* DesiredBaseClass)
	{
		check(DesiredBaseClass);

		FString TargetName = SearchToken;

		bool bIsValidClassName = true;
		if (TargetName.IsEmpty() || TargetName.Contains(TEXT(" ")))
		{
			bIsValidClassName = false;
		}
		else if (!FPackageName::IsShortPackageName(TargetName))
		{
			if (TargetName.Contains(TEXT(".")))
			{
				// 把 type'path' 形式规约为纯路径
				TargetName = FPackageName::ExportTextPathToObjectPath(TargetName);

				FString PackageName;
				FString ObjectName;
				TargetName.Split(TEXT("."), &PackageName, &ObjectName);

				FText Reason;
				if (!FPackageName::IsValidLongPackageName(PackageName, /*bIncludeReadOnlyRoots=*/ true, &Reason))
				{
					bIsValidClassName = false;
				}
			}
			else
			{
				bIsValidClassName = false;
			}
		}

		UClass* ResultClass = bIsValidClassName ? UClass::TryFindTypeSlow<UClass>(TargetName) : nullptr;

		if (ResultClass == nullptr)
		{
			ResultClass = FindBlueprintClassByShortName(TargetName, DesiredBaseClass);
		}

		if (ResultClass == nullptr)
		{
			UE_LOG(LogConsoleResponse, Warning, TEXT("未找到名为 %s 的 %s 类"), *SearchToken, *DesiredBaseClass->GetName());

			return nullptr;
		}

		if (!ResultClass->IsChildOf(DesiredBaseClass))
		{
			UE_LOG(LogConsoleResponse, Warning, TEXT("找到资产 %s，但其类型不是 %s"), *ResultClass->GetPathName(), *DesiredBaseClass->GetName());

			return nullptr;
		}

		return ResultClass;
	}
}

UModularCosmeticCheats::UModularCosmeticCheats()
{
#if UE_WITH_CHEAT_MANAGER
	// 仅 CDO 负责登记，实例由回调按 CheatManager 逐个创建
	if (HasAnyFlags(RF_ClassDefaultObject))
	{
		UCheatManager::RegisterForOnCheatManagerCreated(FOnCheatManagerCreated::FDelegate::CreateLambda(
			[](UCheatManager* CheatManager)
			{
				CheatManager->AddCheatManagerExtension(NewObject<ThisClass>(CheatManager));
			}));
	}
#endif
}

void UModularCosmeticCheats::AddCharacterPart(const FString& AssetName, bool bSuppressNaturalParts)
{
#if UE_WITH_CHEAT_MANAGER
	UModularControllerComponent_CharacterParts* CosmeticComponent = GetCosmeticComponent();
	if (!CosmeticComponent)
	{
		return;
	}

	const TSubclassOf<AActor> PartClass = FindClassByShortName(AssetName, AActor::StaticClass());
	if (PartClass == nullptr)
	{
		return;
	}

	FModularCharacterPart Part;
	Part.PartClass = PartClass;

	CosmeticComponent->AddCheatPart(Part, bSuppressNaturalParts);
#endif
}

void UModularCosmeticCheats::ReplaceCharacterPart(const FString& AssetName, bool bSuppressNaturalParts)
{
	ClearCharacterPartOverrides();
	AddCharacterPart(AssetName, bSuppressNaturalParts);
}

void UModularCosmeticCheats::ClearCharacterPartOverrides()
{
#if UE_WITH_CHEAT_MANAGER
	if (UModularControllerComponent_CharacterParts* CosmeticComponent = GetCosmeticComponent())
	{
		CosmeticComponent->ClearCheatParts();
	}
#endif
}

UModularControllerComponent_CharacterParts* UModularCosmeticCheats::GetCosmeticComponent() const
{
	if (const APlayerController* PC = GetPlayerController())
	{
		return PC->FindComponentByClass<UModularControllerComponent_CharacterParts>();
	}

	return nullptr;
}
