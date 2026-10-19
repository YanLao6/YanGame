// 列表条目控件工厂声明。

#pragma once

#include "CoreMinimal.h"
#include "Templates/SubclassOf.h"
#include "UObject/SoftObjectPtr.h"
#include "ModularWidgetFactory.generated.h"

#define UE_API MODULARGAMEPLAYUI_API

class UUserWidget;

/**
 * 列表条目控件工厂。
 *
 * 让一个列表能按数据项的实际类型选用不同的条目控件，
 * 用于混装多种条目的界面（会话列表、体验列表等）。
 */
UCLASS(MinimalAPI, Abstract, Blueprintable, BlueprintType, EditInlineNew)
class UModularWidgetFactory : public UObject
{
	GENERATED_BODY()

public:
	UModularWidgetFactory() { }

	/** 为给定数据项挑选条目控件类；不适用时返回空。 */
	UFUNCTION(BlueprintNativeEvent)
	UE_API TSubclassOf<UUserWidget> FindWidgetClassForData(const UObject* Data) const;
};

/**
 * 按数据类型查表的控件工厂。
 */
UCLASS(MinimalAPI)
class UModularWidgetFactory_Class : public UModularWidgetFactory
{
	GENERATED_BODY()

public:
	UModularWidgetFactory_Class() { }

	//~Begin UModularWidgetFactory Interface
	UE_API virtual TSubclassOf<UUserWidget> FindWidgetClassForData_Implementation(const UObject* Data) const override;
	//~End UModularWidgetFactory Interface

protected:
	/** 数据类型到条目控件类的映射；沿继承链向上逐级匹配。 */
	UPROPERTY(EditAnywhere, Category = ListEntries, meta = (AllowAbstract))
	TMap<TSoftClassPtr<UObject>, TSubclassOf<UUserWidget>> EntryWidgetForClass;
};

#undef UE_API
