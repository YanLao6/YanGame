// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Engine/DataAsset.h"

#include "ModularTeamDisplayAsset.generated.h"

#define UE_API MODULARTEAMS_API

struct FPropertyChangedEvent;

class AActor;
class UMaterialInstanceDynamic;
class UMeshComponent;
class UNiagaraComponent;
class UTexture;

/** 队伍的展示信息：材质与特效参数、队伍短名。 */
UCLASS(MinimalAPI, BlueprintType)
class UModularTeamDisplayAsset : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TMap<FName, float> ScalarParameters;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TMap<FName, FLinearColor> ColorParameters;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TMap<FName, TObjectPtr<UTexture>> TextureParameters;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FText TeamShortName;

public:
	/** 将全部参数写入动态材质实例。 */
	UFUNCTION(BlueprintCallable, Category = Teams)
	UE_API void ApplyToMaterial(UMaterialInstanceDynamic* Material);

	/** 将全部参数写入网格体组件的材质；纹理参数需要动态材质实例，必要时就地创建。 */
	UFUNCTION(BlueprintCallable, Category = Teams)
	UE_API void ApplyToMeshComponent(UMeshComponent* MeshComponent);

	/** 将全部参数写入 Niagara 用户变量。 */
	UFUNCTION(BlueprintCallable, Category = Teams)
	UE_API void ApplyToNiagaraComponent(UNiagaraComponent* NiagaraComponent);

	/** 遍历 Actor 上的网格体与 Niagara 组件并逐个应用。 */
	UFUNCTION(BlueprintCallable, Category = Teams, meta = (DefaultToSelf = "TargetActor"))
	UE_API void ApplyToActor(AActor* TargetActor, bool bIncludeChildActors = true);

public:
	//~Begin UObject Interface
#if WITH_EDITOR
	UE_API virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
	//~End UObject Interface
};

#undef UE_API
