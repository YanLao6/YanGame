// 可按数据类型生成条目的列表控件实现。

#include "UI/ModularListView.h"

#include "UI/ModularWidgetFactory.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ModularListView)

#if WITH_EDITOR
#include "Editor/WidgetCompilerLog.h"
#endif

#define LOCTEXT_NAMESPACE "ModularListView"

UModularListView::UModularListView(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

#if WITH_EDITOR

void UModularListView::ValidateCompiledDefaults(IWidgetCompilerLog& InCompileLog) const
{
	Super::ValidateCompiledDefaults(InCompileLog);

	if (FactoryRules.Num() == 0)
	{
		InCompileLog.Error(FText::Format(
			LOCTEXT("NoFactoryRules", "{0} 未配置工厂规则，无法决定条目控件的类型。"),
			FText::FromString(GetName())));
	}
}

#endif

UUserWidget& UModularListView::OnGenerateEntryWidgetInternal(UObject* Item, TSubclassOf<UUserWidget> DesiredEntryClass, const TSharedRef<STableViewBase>& OwnerTable)
{
	TSubclassOf<UUserWidget> WidgetClass = DesiredEntryClass;

	for (const UModularWidgetFactory* Rule : FactoryRules)
	{
		if (Rule)
		{
			if (const TSubclassOf<UUserWidget> EntryClass = Rule->FindWidgetClassForData(Item))
			{
				WidgetClass = EntryClass;
				break;
			}
		}
	}

	return GenerateTypedEntry<UUserWidget>(WidgetClass, OwnerTable);
}

#undef LOCTEXT_NAMESPACE
