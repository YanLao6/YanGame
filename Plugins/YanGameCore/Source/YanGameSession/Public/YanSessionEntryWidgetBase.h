// Copyright YanLao.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/IUserObjectListEntry.h"
#include "YanSessionEntryWidgetBase.generated.h"

#define UE_API YANGAMESESSION_API

class UYanSessionSearchResultData;

/**
 * UYanSessionEntryWidgetBase
 *
 * UListView 条目 Widget 的 C++ 基类。
 * 实现了 IUserObjectListEntry（NotBlueprintable，无法在 AngelScript 中直接继承），
 * 并在收到列表项时将数据转发给子类可重写的 BlueprintImplementableEvent OnSessionDataSet。
 *
 * AngelScript 端继承此类（class UYanSessionEntryWidget : UYanSessionEntryWidgetBase），
 * 重写 OnSessionDataSet 来更新 TextBlock 等控件。
 */
UCLASS(MinimalAPI, Abstract, Blueprintable)
class UYanSessionEntryWidgetBase : public UUserWidget, public IUserObjectListEntry
{
	GENERATED_BODY()

protected:
	//~IUserObjectListEntry interface
	/** UListView 将 ListItem 对象传给此 Widget 时自动调用 */
	UE_API virtual void NativeOnListItemObjectSet(UObject* ListItemObject) override;
	//~End

	/**
	 * 收到 UYanSessionSearchResultData 数据时触发，由 AngelScript / Blueprint 子类重写。
	 * 在此函数中更新 PlayerCountText、PingText 等绑定控件。
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Session")
	UE_API void OnSessionDataSet(UYanSessionSearchResultData* Data);
};

#undef UE_API
