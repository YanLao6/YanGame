// 通用系统静态库声明。

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "UObject/SoftObjectPtr.h"
#include "ModularSystemStatics.generated.h"

#define UE_API MODULARGAMEPLAYEXPERIENCES_API

template <typename T> class TSubclassOf;

class AActor;
class UActorComponent;

/**
 * 通用系统静态库。
 *
 * 汇集资产标识解析、关卡切换与材质参数批量设置等在蓝图中频繁使用的操作。
 */
UCLASS(MinimalAPI)
class UModularSystemStatics : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** 由 PrimaryAssetId 取软引用；资产未加载时同样可用。 */
	UFUNCTION(BlueprintPure, Category = "AssetManager", meta=(DeterminesOutputType=ExpectedAssetType))
	static UE_API TSoftObjectPtr<UObject> GetTypedSoftObjectReferenceFromPrimaryAssetId(FPrimaryAssetId PrimaryAssetId, TSubclassOf<UObject> ExpectedAssetType);

	/** 由对外公布的体验名拼出其 PrimaryAssetId。 */
	UFUNCTION(BlueprintCallable)
	static UE_API FPrimaryAssetId GetPrimaryAssetIdFromUserFacingExperienceName(const FString& AdvertisedExperienceID);

	/** 以无缝切换的方式重开当前关卡，保持客户端连接。 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Modular", meta = (WorldContext = "WorldContextObject"))
	static UE_API void PlayNextGame(const UObject* WorldContextObject);

	/** 对目标 Actor 上所有网格组件的全部材质设置标量参数。 */
	UFUNCTION(BlueprintCallable, Category = "Rendering|Material", meta=(DefaultToSelf="TargetActor"))
	static UE_API void SetScalarParameterValueOnAllMeshComponents(AActor* TargetActor, const FName ParameterName, const float ParameterValue, bool bIncludeChildActors = true);

	/** 对目标 Actor 上所有网格组件的全部材质设置向量参数。 */
	UFUNCTION(BlueprintCallable, Category = "Rendering|Material", meta=(DefaultToSelf="TargetActor"))
	static UE_API void SetVectorParameterValueOnAllMeshComponents(AActor* TargetActor, const FName ParameterName, const FVector ParameterValue, bool bIncludeChildActors = true);

	/** 对目标 Actor 上所有网格组件的全部材质设置颜色参数。 */
	UFUNCTION(BlueprintCallable, Category = "Rendering|Material", meta=(DefaultToSelf="TargetActor"))
	static UE_API void SetColorParameterValueOnAllMeshComponents(AActor* TargetActor, const FName ParameterName, const FLinearColor ParameterValue, bool bIncludeChildActors = true);

	/** 取目标 Actor 上派生自指定类的全部组件。 */
	UFUNCTION(BlueprintCallable, Category = "Actor", meta=(DefaultToSelf="TargetActor", ComponentClass="/Script/Engine.ActorComponent", DeterminesOutputType="ComponentClass"))
	static UE_API TArray<UActorComponent*> FindComponentsByClass(AActor* TargetActor, TSubclassOf<UActorComponent> ComponentClass, bool bIncludeChildActors = true);
};

#undef UE_API
