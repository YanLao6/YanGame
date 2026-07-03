// 版权声明请在「项目设置 → 描述」中填写。


#include "Player/ModularAbilityPlayerController.h"

#include "ModularAbilityPlayerState.h"
#include "ModularPlayerState.h"


#include UE_INLINE_GENERATED_CPP_BY_NAME(ModularAbilityPlayerController)

AModularAbilityPlayerState* AModularAbilityPlayerController::GetModularPlayerState() const
{
	return CastChecked<AModularAbilityPlayerState>(PlayerState, ECastCheckedType::NullAllowed);
}

UModularAbilitySystemComponent* AModularAbilityPlayerController::GetModularAbilitySystemComponent() const
{
	const AModularAbilityPlayerState* ModularPS = GetModularPlayerState();
	return (ModularPS ? ModularPS->GetModularAbilitySystemComponent() : nullptr);
}

void AModularAbilityPlayerController::PreProcessInput(const float DeltaTime, const bool bGamePaused)
{
	Super::PreProcessInput(DeltaTime, bGamePaused);
}

void AModularAbilityPlayerController::PostProcessInput(const float DeltaTime, const bool bGamePaused)
{
	if (UModularAbilitySystemComponent* ModularASC = GetModularAbilitySystemComponent())
	{
		ModularASC->ProcessAbilityInput(DeltaTime, bGamePaused);
	}
	
	Super::PostProcessInput(DeltaTime, bGamePaused);
	
}
