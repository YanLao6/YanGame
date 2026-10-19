// 列表条目控件工厂实现。

#include "UI/ModularWidgetFactory.h"

#include "Blueprint/UserWidget.h"
#include "Templates/SubclassOf.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ModularWidgetFactory)

TSubclassOf<UUserWidget> UModularWidgetFactory::FindWidgetClassForData_Implementation(const UObject* Data) const
{
	return TSubclassOf<UUserWidget>();
}

TSubclassOf<UUserWidget> UModularWidgetFactory_Class::FindWidgetClassForData_Implementation(const UObject* Data) const
{
	// 自数据项的实际类型起沿继承链向上找，取第一条命中的规则
	for (UClass* Class = Data->GetClass(); Class; Class = Class->GetSuperClass())
	{
		const TSoftClassPtr<UObject> ClassPtr(Class);
		if (const TSubclassOf<UUserWidget> EntryWidgetClassPtr = EntryWidgetForClass.FindRef(ClassPtr))
		{
			return EntryWidgetClassPtr;
		}
	}

	return TSubclassOf<UUserWidget>();
}
