#include "NumberPops/YanNumberPopComponent_NiagaraText.h"

#include "NiagaraComponent.h"
#include "NiagaraDataInterfaceArrayFunctionLibrary.h"
#include "NumberPops/YanDamagePopStyleNiagara.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(YanNumberPopComponent_NiagaraText)

UYanNumberPopComponent_NiagaraText::UYanNumberPopComponent_NiagaraText(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{}

void UYanNumberPopComponent_NiagaraText::AddNumberPop(const FYanNumberPopRequest& NewRequest)
{
	if (!Style)
	{
		return;
	}

	// 以负数区分暴击，粒子侧据此切换表现
	const int32 LocalDamage = NewRequest.bIsCriticalDamage ? -NewRequest.NumberToDisplay : NewRequest.NumberToDisplay;

	if (!NiagaraComp)
	{
		NiagaraComp = NewObject<UNiagaraComponent>(GetOwner());
		NiagaraComp->SetAsset(Style->TextNiagara);
		NiagaraComp->bAutoActivate = false;
		NiagaraComp->SetupAttachment(nullptr);
		NiagaraComp->RegisterComponent();
	}

	NiagaraComp->Activate(false);
	NiagaraComp->SetWorldLocation(NewRequest.WorldLocation);

	// 伤害信息打包进 FVector4：XYZ 为位置，W 为数值
	TArray<FVector4> DamageList = UNiagaraDataInterfaceArrayFunctionLibrary::GetNiagaraArrayVector4(NiagaraComp, Style->NiagaraArrayName);
	DamageList.Add(FVector4(NewRequest.WorldLocation.X, NewRequest.WorldLocation.Y, NewRequest.WorldLocation.Z, LocalDamage));
	UNiagaraDataInterfaceArrayFunctionLibrary::SetNiagaraArrayVector4(NiagaraComp, Style->NiagaraArrayName, DamageList);
}
