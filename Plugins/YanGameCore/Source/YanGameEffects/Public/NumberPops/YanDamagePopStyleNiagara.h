#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"

#include "YanDamagePopStyleNiagara.generated.h"

class UNiagaraSystem;

/**
 * Niagara 飘字的样式，指定粒子系统与接收伤害数据的数组参数名。
 */
UCLASS(MinimalAPI)
class UYanDamagePopStyleNiagara : public UDataAsset
{
	GENERATED_BODY()

public:
	// 粒子系统中接收伤害数据的 Vector4 数组参数名
	UPROPERTY(EditDefaultsOnly, Category = "DamagePop")
	FName NiagaraArrayName;

	// 负责绘制飘字的粒子系统
	UPROPERTY(EditDefaultsOnly, Category = "DamagePop")
	TObjectPtr<UNiagaraSystem> TextNiagara;
};
