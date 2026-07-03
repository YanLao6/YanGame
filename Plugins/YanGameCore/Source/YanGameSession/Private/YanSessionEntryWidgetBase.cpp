// Copyright YanLao.

#include "YanSessionEntryWidgetBase.h"
#include "YanSessionSearchResultData.h"

void UYanSessionEntryWidgetBase::NativeOnListItemObjectSet(UObject* ListItemObject)
{
	// 先调用基类（处理内部 IUserObjectListEntry 状态）
	IUserObjectListEntry::NativeOnListItemObjectSet(ListItemObject);

	// 将数据转发给 AngelScript / Blueprint 子类
	if (UYanSessionSearchResultData* Data = Cast<UYanSessionSearchResultData>(ListItemObject))
	{
		OnSessionDataSet(Data);
	}
}
