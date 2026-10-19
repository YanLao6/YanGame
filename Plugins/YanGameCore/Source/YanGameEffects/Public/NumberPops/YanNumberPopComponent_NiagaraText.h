#pragma once

#include "NumberPops/YanNumberPopComponent.h"

#include "YanNumberPopComponent_NiagaraText.generated.h"

#define UE_API YANGAMEEFFECTS_API

class UYanDamagePopStyleNiagara;
class UNiagaraComponent;

/**
 * 以 Niagara 粒子绘制伤害飘字。
 * 所有数值汇入同一个粒子系统的数组参数，由粒子自行排布与消散。
 */
UCLASS(MinimalAPI, Blueprintable)
class UYanNumberPopComponent_NiagaraText : public UYanNumberPopComponent
{
	GENERATED_BODY()

public:
	UE_API UYanNumberPopComponent_NiagaraText(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	//~Begin UYanNumberPopComponent Interface
	UE_API virtual void AddNumberPop(const FYanNumberPopRequest& NewRequest) override;
	//~End UYanNumberPopComponent Interface

protected:
	UPROPERTY(EditDefaultsOnly, Category = "Number Pop|Style")
	TObjectPtr<UYanDamagePopStyleNiagara> Style;

	// 首次飘字时惰性创建，之后复用
	UPROPERTY(Transient)
	TObjectPtr<UNiagaraComponent> NiagaraComp;
};

#undef UE_API
