/**
 * EquipmentSlotView — 槽位列表同步工具。
 *
 * 把"按物品数组生成/复用/更新 UW_ItemSlot 子控件"抽成一处，
 * 供快捷栏、背包与轮盘等界面复用。数量变化时重建子控件，数量稳定时原地更新，
 * 避免轮询刷新下反复销毁重建带来的闪烁与 GC 压力。
 *
 * 本工具只管子控件的增删与内容同步，不涉及排布：容器自身的布局规则
 * （HorizontalBox 的排列、CanvasPanel 的坐标等）由调用方负责。
 */
namespace EquipmentSlotView
{
	/**
	 * 将 Items 同步到 Container 中的槽位子控件。
	 *
	 * @param Owner       发起方 widget，用于取 OwningPlayer 作为子控件的拥有者
	 * @param Container    槽位容器
	 * @param SlotClass    槽位子控件类
	 * @param Pool         已生成的子控件池（原地增删，与 Container 子节点保持一致）
	 * @param Items        待呈现的物品数组，null 元素表示空槽
	 * @param ActiveIndex  需高亮项在 Items 中的下标；传 -1 表示无激活项
	 * @param KeySlotIndices 呈现位到快捷栏槽位索引的映射，槽位据此显示按键提示。
	 *                       快捷栏可只呈现部分槽位，故呈现位与槽位索引不必相等；
	 *                       留空表示整列与快捷栏无关（背包、轮盘），一律不显示按键。
	 *
	 * @return 全部槽位的图标是否均已就绪；为 false 表示仍有图标在异步流送，
	 *         事件驱动的调用方需据此挂起重试，轮询驱动的调用方可忽略。
	 */
	bool Sync(
		UUserWidget Owner,
		UPanelWidget Container,
		TSubclassOf<UUserWidget> SlotClass,
		TArray<UW_ItemSlot>& Pool,
		TArray<UInventoryItemInstance> Items,
		int32 ActiveIndex,
		TArray<int32> KeySlotIndices)
	{
		if (Owner == nullptr || Container == nullptr || SlotClass.Get() == nullptr)
		{
			return false;
		}

		const int32 Desired = Items.Num();

		if (Pool.Num() != Desired)
		{
			Container.ClearChildren();
			Pool.Empty();

			APlayerController OwningController = Owner.GetOwningPlayer();
			for (int32 Index = 0; Index < Desired; Index++)
			{
				UW_ItemSlot Slot = Cast<UW_ItemSlot>(WidgetBlueprint::CreateWidget(SlotClass, OwningController));
				if (Slot == nullptr)
				{
					continue;
				}
				Container.AddChild(Slot);
				Pool.Add(Slot);
			}
		}

		bool bAllReady = true;
		for (int32 Index = 0; Index < Pool.Num(); Index++)
		{
			UInventoryItemInstance Item = Index < Items.Num() ? Items[Index] : nullptr;
			const int32 KeySlotIndex = KeySlotIndices.IsValidIndex(Index) ? KeySlotIndices[Index] : -1;
			if (!Pool[Index].SetSlotItem(Item, Index == ActiveIndex, KeySlotIndex))
			{
				bAllReady = false;
			}
		}
		return bAllReady;
	}
}
