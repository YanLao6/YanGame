// 可按数据类型生成条目的列表控件声明。

#pragma once

#include "CoreMinimal.h"
#include "CommonListView.h"
#include "ModularListView.generated.h"

#define UE_API MODULARGAMEPLAYUI_API

class UUserWidget;
class UModularWidgetFactory;

/**
 * 可按数据类型生成条目的列表控件。
 *
 * 在通用列表之上引入工厂规则：条目控件的类型由数据项决定，
 * 使一个列表能混装多种条目。
 */
UCLASS(MinimalAPI, meta = (DisableNativeTick))
class UModularListView : public UCommonListView
{
	GENERATED_BODY()

public:
	UE_API explicit UModularListView(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

#if WITH_EDITOR
	//~Begin UWidget Interface
	UE_API virtual void ValidateCompiledDefaults(IWidgetCompilerLog& InCompileLog) const override;
	//~End UWidget Interface
#endif

protected:
	//~Begin UListView Interface
	UE_API virtual UUserWidget& OnGenerateEntryWidgetInternal(UObject* Item, TSubclassOf<UUserWidget> DesiredEntryClass, const TSharedRef<STableViewBase>& OwnerTable) override;
	//~End UListView Interface

	/** 条目控件的选取规则，按顺序取第一条命中的。 */
	UPROPERTY(EditAnywhere, Instanced, Category="Entry Creation")
	TArray<TObjectPtr<UModularWidgetFactory>> FactoryRules;
};

#undef UE_API
