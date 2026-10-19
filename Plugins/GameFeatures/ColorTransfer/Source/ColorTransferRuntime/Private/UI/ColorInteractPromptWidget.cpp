#include "UI/ColorInteractPromptWidget.h"

#include "Components/TextBlock.h"
#include "ModularExperienceUtils.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ColorInteractPromptWidget)

void UColorInteractPromptWidget::NativeConstruct()
{
	Super::NativeConstruct();

	const FKey Key = UModularExperienceUtils::GetKeyValueByTag(GetOwningPlayer(), InputTag);
	KeyText->SetText(Key.IsValid() ? Key.GetDisplayName(/*bLongDisplayName=*/false) : FText::GetEmpty());
}
