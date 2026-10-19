// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CommonTabListWidgetBase.h"

#include "ModularTabListWidgetBase.generated.h"

#define UE_API MODULARGAMEPLAYUI_API

class UCommonButtonBase;
class UCommonUserWidget;
class UObject;
class UWidget;

/** 一个页签的描述：按钮外观、对应内容控件，以及运行时创建出的内容实例。 */
USTRUCT(BlueprintType)
struct FModularTabDescriptor
{
	GENERATED_BODY()

public:
	FModularTabDescriptor()
		: bHidden(false)
		, CreatedTabContentWidget(nullptr)
	{
	}

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FName TabId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FText TabText;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FSlateBrush IconBrush;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	bool bHidden;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TSubclassOf<UCommonButtonBase> TabButtonType;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TSubclassOf<UCommonUserWidget> TabContentType;

	UPROPERTY(Transient)
	TObjectPtr<UWidget> CreatedTabContentWidget;
};

UINTERFACE(BlueprintType)
class UModularTabButtonInterface : public UInterface
{
	GENERATED_BODY()
};

/** 页签按钮据此接收自身的标题与图标 */
class IModularTabButtonInterface
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintNativeEvent, Category = "Tab Button")
	void SetTabLabelInfo(const FModularTabDescriptor& TabDescriptor);
};

/**
 * 页签列表。页签可在资产上预先登记，也可在运行时动态注册；
 * 内容控件在页签建立时创建，并挂到关联的 Switcher 上。
 */
UCLASS(MinimalAPI, Blueprintable, BlueprintType, Abstract, meta = (DisableNativeTick))
class UModularTabListWidgetBase : public UCommonTabListWidgetBase
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Tab List")
	UE_API bool GetPreregisteredTabInfo(const FName TabNameId, FModularTabDescriptor& OutTabInfo);

	/** 取全部预登记页签信息 */
	const TArray<FModularTabDescriptor>& GetAllPreregisteredTabInfos() { return PreregisteredTabInfoArray; }

	/** 切换指定页签的隐藏状态，只能在关联 Switcher 之前调用 */
	UFUNCTION(BlueprintCallable, Category = "Tab List")
	UE_API void SetTabHiddenState(FName TabNameId, bool bHidden);

	UFUNCTION(BlueprintCallable, Category = "Tab List")
	UE_API bool RegisterDynamicTab(const FModularTabDescriptor& TabDescriptor);

	UFUNCTION(BlueprintCallable, Category = "Tab List")
	UE_API bool IsFirstTabActive() const;

	UFUNCTION(BlueprintCallable, Category = "Tab List")
	UE_API bool IsLastTabActive() const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Tab List")
	UE_API bool IsTabVisible(FName TabId);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Tab List")
	UE_API int32 GetVisibleTabCount();

	DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnTabContentCreated, FName, TabId, UCommonUserWidget*, TabWidget);
	DECLARE_EVENT_TwoParams(UModularTabListWidgetBase, FOnTabContentCreatedNative, FName /* TabId */, UCommonUserWidget* /* TabWidget */);

	/** 新页签内容创建完成时广播，便于外部挂接 */
	UPROPERTY(BlueprintAssignable, Category = "Tab List")
	FOnTabContentCreated OnTabContentCreated;
	FOnTabContentCreatedNative OnTabContentCreatedNative;

protected:
	//~Begin UUserWidget Interface
	UE_API virtual void NativeConstruct() override;
	UE_API virtual void NativeDestruct() override;
	//~End UUserWidget Interface

	//~Begin UCommonTabListWidgetBase Interface
	UE_API virtual void HandlePreLinkedSwitcherChanged() override;
	UE_API virtual void HandlePostLinkedSwitcherChanged() override;
	UE_API virtual void HandleTabCreation_Implementation(FName TabId, UCommonButtonBase* TabButton) override;
	//~End UCommonTabListWidgetBase Interface

private:
	void SetupTabs();

	UPROPERTY(EditAnywhere, meta = (TitleProperty = "TabId"))
	TArray<FModularTabDescriptor> PreregisteredTabInfoArray;

	/** 运行时注册但尚未创建出按钮的页签信息，创建后移除 */
	UPROPERTY()
	TMap<FName, FModularTabDescriptor> PendingTabLabelInfoMap;
};

#undef UE_API
