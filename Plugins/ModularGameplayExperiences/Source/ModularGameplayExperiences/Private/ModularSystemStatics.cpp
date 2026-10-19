// 通用系统静态库实现。

#include "ModularSystemStatics.h"

#include "Components/MeshComponent.h"
#include "GameMode/ModularUserFacingExperienceDefinition.h"
#include "Engine/AssetManager.h"
#include "Engine/AssetManagerTypes.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "ModularGameplayExperiencesLogs.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ModularSystemStatics)

TSoftObjectPtr<UObject> UModularSystemStatics::GetTypedSoftObjectReferenceFromPrimaryAssetId(FPrimaryAssetId PrimaryAssetId, TSubclassOf<UObject> ExpectedAssetType)
{
	if (UAssetManager* Manager = UAssetManager::GetIfInitialized())
	{
		FPrimaryAssetTypeInfo Info;
		if (Manager->GetPrimaryAssetTypeInfo(PrimaryAssetId.PrimaryAssetType, Info) && !Info.bHasBlueprintClasses)
		{
			if (const UClass* AssetClass = Info.AssetBaseClassLoaded)
			{
				if ((ExpectedAssetType == nullptr) || !AssetClass->IsChildOf(ExpectedAssetType))
				{
					return nullptr;
				}
			}
			else
			{
				UE_LOG(LogModularGameplayExperiences, Warning, TEXT("由 PrimaryAssetId (%s, %s) 取软引用：基类未加载，无法校验类型，返回空。"),
					*PrimaryAssetId.ToString(),
					*GetPathNameSafe(*ExpectedAssetType));
			}

			return TSoftObjectPtr<UObject>(Manager->GetPrimaryAssetPath(PrimaryAssetId));
		}
	}
	return nullptr;
}

FPrimaryAssetId UModularSystemStatics::GetPrimaryAssetIdFromUserFacingExperienceName(const FString& AdvertisedExperienceID)
{
	const FPrimaryAssetType Type(UModularUserFacingExperienceDefinition::StaticClass()->GetFName());
	return FPrimaryAssetId(Type, FName(*AdvertisedExperienceID));
}

void UModularSystemStatics::PlayNextGame(const UObject* WorldContextObject)
{
	UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
	if (World == nullptr)
	{
		return;
	}

	const FWorldContext& WorldContext = GEngine->GetWorldContextFromWorldChecked(World);
	FURL LastURL = WorldContext.LastURL;

#if WITH_EDITOR
	// PIE 下的关卡名带前缀，切换前须剥离
	LastURL.Map = UWorld::StripPIEPrefixFromPackageName(LastURL.Map, WorldContext.World()->StreamingLevelsPrefix);
#endif

	// 请求无缝切换以保持客户端连接；无缝切换被禁用时会自动退化为普通切换
	LastURL.AddOption(TEXT("SeamlessTravel"));

	FString URL = LastURL.ToString();
	// 保留主机与端口信息会导致 ServerTravel 失败
	URL.RemoveFromStart(LastURL.GetHostPortString());

	const bool bAbsolute = false;
	const bool bShouldSkipGameNotify = false;
	World->ServerTravel(URL, bAbsolute, bShouldSkipGameNotify);
}

void UModularSystemStatics::SetScalarParameterValueOnAllMeshComponents(AActor* TargetActor, const FName ParameterName, const float ParameterValue, bool bIncludeChildActors)
{
	if (TargetActor != nullptr)
	{
		TargetActor->ForEachComponent<UMeshComponent>(bIncludeChildActors, [=](UMeshComponent* InComponent)
		{
			InComponent->SetScalarParameterValueOnMaterials(ParameterName, ParameterValue);
		});
	}
}

void UModularSystemStatics::SetVectorParameterValueOnAllMeshComponents(AActor* TargetActor, const FName ParameterName, const FVector ParameterValue, bool bIncludeChildActors)
{
	if (TargetActor != nullptr)
	{
		TargetActor->ForEachComponent<UMeshComponent>(bIncludeChildActors, [=](UMeshComponent* InComponent)
		{
			InComponent->SetVectorParameterValueOnMaterials(ParameterName, ParameterValue);
		});
	}
}

void UModularSystemStatics::SetColorParameterValueOnAllMeshComponents(AActor* TargetActor, const FName ParameterName, const FLinearColor ParameterValue, bool bIncludeChildActors)
{
	SetVectorParameterValueOnAllMeshComponents(TargetActor, ParameterName, FVector(ParameterValue), bIncludeChildActors);
}

TArray<UActorComponent*> UModularSystemStatics::FindComponentsByClass(AActor* TargetActor, TSubclassOf<UActorComponent> ComponentClass, bool bIncludeChildActors)
{
	TArray<UActorComponent*> Components;
	if (TargetActor != nullptr)
	{
		TargetActor->GetComponents(ComponentClass, /*out*/ Components, bIncludeChildActors);
	}
	return MoveTemp(Components);
}
